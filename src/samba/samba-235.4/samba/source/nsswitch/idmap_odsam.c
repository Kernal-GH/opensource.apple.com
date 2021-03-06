/*
   Unix SMB/CIFS implementation.
   ID mapping Open Directory Server backend

   Copyright (c) 2007-2009 Apple Inc. All rights rserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include "includes.h"
#include "winbindd.h"
#include "opendirectory.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/* NOTE: The Open Directory passdb and idmap modules are tightly coupled, so we
 * link them into the same DSO and expect them to be used together.
 */
#define MODULE_NAME "odsam"

static enum ds_trace_level ds_trace = DS_TRACE_ERRORS;
static int module_debug;

static struct opendirectory_session od_idmap_session;

/* Map a SID to the corresponding Unix UID or GID. */
static NTSTATUS ods_map_sid(struct opendirectory_session *session,
				struct id_map *id_entry)
{
	CFDictionaryRef sam_record;
	char * id_string;

	id_entry->status = ID_UNKNOWN;

	if (id_entry->xid.type == ID_TYPE_UID) {
		sam_record = opendirectory_find_record_from_usersid(session,
							id_entry->sid);
		if (sam_record) {

			id_string = opendirectory_get_record_attribute(NULL,
					    sam_record, kDS1AttrUniqueID);

			goto success;
		}
	} else {
		SMB_ASSERT(id_entry->xid.type == ID_TYPE_GID);
		sam_record = opendirectory_find_record_from_groupsid(session,
							id_entry->sid);
		if (sam_record) {
			id_entry->xid.type = ID_TYPE_GID;
			id_string = opendirectory_get_record_attribute(NULL,
					    sam_record, kDS1AttrPrimaryGroupID);
			goto success;
		}

		DEBUG(module_debug, ("%s: no match for %s\n", MODULE_NAME,
			sid_string_static(id_entry->sid)));
	}

	return NT_STATUS_OK;

success:
	DEBUG(module_debug, ("%s: mapped %s SID to %s ID %s\n",
		    MODULE_NAME,
		    (id_entry->xid.type == ID_TYPE_UID) ? "user" : "group",
		    sid_string_static(id_entry->sid),
		    id_string));


	id_entry->status = ID_MAPPED;
	id_entry->xid.id = strtoul(id_string, NULL, 10 /* base */);
	TALLOC_FREE(id_string);

	CFRelease(sam_record);
	return NT_STATUS_OK;
}

static NTSTATUS memberd_map_sid(struct id_map *id_entry)
{
	uuid_t uuid;
	id_t ugid;
	int which;
	int err;

	id_entry->status = ID_UNMAPPED;

	if (!memberd_sid_to_uuid(id_entry->sid, uuid)) {
		return NT_STATUS_NONE_MAPPED;
	}

	err = mbr_uuid_to_id(uuid, &ugid, &which);
	if (err != 0) {
		if (DEBUGLVL(6)) {
			uuid_string_t str;
			uuid_unparse(uuid, str);

			DEBUGADD(6,
				("%s: unable to map UUID %s to a SID: %s\n",
				MODULE_NAME, str, strerror(err)));
		}

		return NT_STATUS_NONE_MAPPED;
	}

	if (id_entry->xid.type == ID_TYPE_UID && which == MBR_ID_TYPE_UID) {
		id_entry->xid.id = ugid;
		goto success;
	}

	if (id_entry->xid.type == ID_TYPE_GID && which == MBR_ID_TYPE_GID) {
		id_entry->xid.id = ugid;
		goto success;
	}

	/* This is bad. Samba and DirectoryService disagree on what type of
	 * record the SID refers to. Something is messed up ...
	 */
	DEBUG(module_debug,
		("%s: ID %s SID %s turned out to be of type %d\n",
		 MODULE_NAME,
		 (id_entry->xid.type == ID_TYPE_UID) ? "user" : "group",
		 sid_string_static(id_entry->sid), which));

	return NT_STATUS_INVALID_SID;

success:
	id_entry->status = ID_MAPPED;
	return NT_STATUS_OK;
}

static NTSTATUS memberd_map_unixid(struct id_map *id_entry)
{
	uuid_t uuid;
	int err;

	id_entry->status = ID_UNMAPPED;

	if (id_entry->xid.type == ID_TYPE_UID) {
	    err = mbr_identifier_to_uuid(MBR_ID_TYPE_UID, &id_entry->xid.id,
		    sizeof(id_entry->xid.id), uuid);
	} else {
	    err = mbr_identifier_to_uuid(MBR_ID_TYPE_GID, &id_entry->xid.id,
		    sizeof(id_entry->xid.id), uuid);
	}

	if (err) {
		DEBUG(module_debug, ("%s:unable to map %s %d to a UUID: %s\n",
			MODULE_NAME,
			id_entry->xid.type == ID_TYPE_UID ? "UID" : "GID",
			id_entry->xid.id, strerror(err)));
		return map_nt_error_from_unix(err);
	}

	if (!memberd_uuid_to_sid(uuid, id_entry->sid)) {
		return NT_STATUS_NONE_MAPPED;
	}

	id_entry->status = ID_MAPPED;

	DEBUG(module_debug, ("%s: mapped %s ID %li to SID %s\n",
		    MODULE_NAME,
		    (id_entry->xid.type == ID_TYPE_UID) ? "user" : "group",
		    (long)id_entry->xid.id,
		    sid_string_static(id_entry->sid)));

	return NT_STATUS_OK;
}

/* Map a Unix UID or GID to the corresponding SID. */
static NTSTATUS ods_map_unixid(struct opendirectory_session *session,
				struct id_map *id_entry)
{
	CFDictionaryRef sam_record;

	id_entry->status = ID_UNKNOWN;

	if (id_entry->xid.type == ID_TYPE_UID) {
		sam_record = opendirectory_sam_searchugid_first(session,
				    kDSStdRecordTypeUsers,
				    kDS1AttrUniqueID,
				    id_entry->xid.id);
	} else {
		SMB_ASSERT(id_entry->xid.type == ID_TYPE_GID);
		sam_record = opendirectory_sam_searchugid_first(session,
				    kDSStdRecordTypeGroups,
				    kDS1AttrPrimaryGroupID,
				    id_entry->xid.id);
	}

	if (!sam_record) {
		DEBUG(module_debug, ("%s: no match for %s ID %li\n",
		    MODULE_NAME,
		    (id_entry->xid.type == ID_TYPE_UID) ? "user" : "group",
		    (long)id_entry->xid.id));

		return NT_STATUS_OK;
	}

	id_entry->status = ID_UNMAPPED;

	if (id_entry->xid.type == ID_TYPE_UID) {
		if (opendirectory_find_usersid_from_record(session,
					    sam_record, id_entry->sid)) {
			id_entry->status = ID_MAPPED;
		}
	} else {
		SMB_ASSERT(id_entry->xid.type == ID_TYPE_GID);
		if (opendirectory_find_groupsid_from_record(session,
					    sam_record, id_entry->sid)) {
			id_entry->status = ID_MAPPED;
		}
	}

	DEBUG(module_debug, ("%s: mapped %s ID %li to SID %s\n",
		    MODULE_NAME,
		    (id_entry->xid.type == ID_TYPE_UID) ? "user" : "group",
		    (long)id_entry->xid.id,
		    sid_string_static(id_entry->sid)));

	CFRelease(sam_record);
	return NT_STATUS_OK;
}

static NTSTATUS idmap_ods_initialize(struct idmap_domain *dom)
{
	if (opendirectory_connect(&od_idmap_session) != eDSNoErr) {
		/* XXX should be mapping DS to NT errors */
		return NT_STATUS_INSUFF_SERVER_RESOURCES;
	}

	return NT_STATUS_OK;
}

static NTSTATUS idmap_ods_close(struct idmap_domain *dom)
{
	opendirectory_disconnect(&od_idmap_session);
	return NT_STATUS_OK;
}

static NTSTATUS idmap_ods_unixids_to_sids(struct idmap_domain *dom,
				    struct id_map **ids)
{
	struct id_map **current;

	if (opendirectory_reconnect(&od_idmap_session) != eDSNoErr) {
		/* XXX should be mapping DS to NT errors */
		return NT_STATUS_INSUFF_SERVER_RESOURCES;
	}

	for (current = ids; current && *current; ++current) {
		NTSTATUS s;

		s = memberd_map_unixid(*current);
		if (!NT_STATUS_EQUAL(s, NT_STATUS_OK)) {
			ods_map_unixid(&od_idmap_session, *current);
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS idmap_ods_sids_to_unixids(struct idmap_domain *dom,
				    struct id_map **ids)
{
	struct id_map **current;

	if (opendirectory_reconnect(&od_idmap_session) != eDSNoErr) {
		/* XXX should be mapping DS to NT errors */
		return NT_STATUS_INSUFF_SERVER_RESOURCES;
	}

	for (current = ids; current && *current; ++current) {
		NTSTATUS s;

		s = memberd_map_sid(*current);
		if (!NT_STATUS_EQUAL(s, NT_STATUS_OK)) {
			ods_map_sid(&od_idmap_session, *current);
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS ods_alloc_init(const char * compat)
{
	return NT_STATUS_OK;
}

static NTSTATUS ods_alloc_close(void)
{
	return NT_STATUS_OK;
}

static NTSTATUS ods_alloc_not_supported(struct unixid *id)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static struct idmap_methods ods_idmap_methods = {
	/* init */		idmap_ods_initialize,
	/* unixids_to_sids */	idmap_ods_unixids_to_sids,
	/* sids_to_unixids */	idmap_ods_sids_to_unixids,
	/* set_mapping */	NULL,
	/* remove_mapping */	NULL,
	/* dump_data */		NULL,
	/* close */		idmap_ods_close
};

/* Any static mappings are controlled in Open Directory using the native tools.
 * We provide a module that always fails to make it explicit that we can't
 * manipulate this from the Samba side.
 */
static struct idmap_alloc_methods ods_alloc_methods = {

	/* init */	    ods_alloc_init,
	/* allocate_id */   ods_alloc_not_supported,
	/* get_id_hwm */    ods_alloc_not_supported,
	/* set_id_hwm */    ods_alloc_not_supported,
	/* close */	    ods_alloc_close
};

 NTSTATUS idmap_odsam_init(void)
{
	/* Use "odsam:traceall = yes" to turn on OD query tracing. */
	if (lp_parm_bool(GLOBAL_SECTION_SNUM,
				MODULE_NAME, "traceall", False)) {
		ds_trace = DS_TRACE_ALL;
	}

	module_debug = lp_parm_int(GLOBAL_SECTION_SNUM,
				    MODULE_NAME, "msglevel", 100);

	smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, MODULE_NAME,
		&ods_idmap_methods);
	smb_register_idmap_alloc(SMB_IDMAP_INTERFACE_VERSION, MODULE_NAME,
		&ods_alloc_methods);
	return NT_STATUS_OK;
}

