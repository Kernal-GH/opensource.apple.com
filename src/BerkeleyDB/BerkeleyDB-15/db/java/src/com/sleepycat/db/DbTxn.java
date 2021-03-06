/*
 *  ----------------------------------------------------------------------------
 *  This file was automatically generated by SWIG (http://www.swig.org).
 *  Version: 1.3.19
 *
 *  Do not make changes to this file unless you know what you are doing--modify
 *  the SWIG interface file instead.
 *  -----------------------------------------------------------------------------
 */
package com.sleepycat.db;

/**
 *  The DbTxn object is the handle for a transaction. Methods off the
 *  DbTxn handle are used to configure, abort and commit the
 *  transaction. DbTxn handles are provided to {@link
 *  com.sleepycat.db.Db Db} methods in order to transactionally
 *  protect those database operations.</p> <p>
 *
 *  DbTxn handles are not free-threaded; transactions handles may be
 *  used by multiple threads, but only serially, that is, the
 *  application must serialize access to the DbTxn handle. Once the
 *  {@link com.sleepycat.db.DbTxn#abort DbTxn.abort} or {@link
 *  com.sleepycat.db.DbTxn#commit DbTxn.commit} methods are called,
 *  the handle may not be accessed again, regardless of the method's
 *  return. In addition, parent transactions may not issue any
 *  Berkeley DB operations while they have active child transactions
 *  (child transactions that have not yet been committed or aborted)
 *  except for {@link com.sleepycat.db.DbEnv#txnBegin DbEnv.txnBegin},
 *  {@link com.sleepycat.db.DbTxn#abort DbTxn.abort} and {@link
 *  com.sleepycat.db.DbTxn#commit DbTxn.commit}.</p>
 */
public class DbTxn {
    private long swigCPtr;
    protected boolean swigCMemOwn;


    protected DbTxn(long cPtr, boolean cMemoryOwn) {
        swigCMemOwn = cMemoryOwn;
        swigCPtr = cPtr;
    }


    protected DbTxn() {
        this(0, false);
    }


    protected void delete() {
        if (swigCPtr != 0 && swigCMemOwn) {
            swigCMemOwn = false;
        }
        swigCPtr = 0;
    }


    protected static long getCPtr(DbTxn obj) {
        return (obj == null) ? 0 : obj.swigCPtr;
    }


    /**
     *  The DbTxn.abort method causes an abnormal termination of the
     *  transaction. The log is played backward, and any necessary
     *  undo operations are done through the <b>tx_recover</b>
     *  function specified to {@link com.sleepycat.db.DbEnv#setAppDispatch
     *  DbEnv.setAppDispatch}. Before DbTxn.abort returns, any locks
     *  held by the transaction will have been released.</p> <p>
     *
     *  In the case of nested transactions, aborting a parent
     *  transaction causes all children (unresolved or not) of the
     *  parent transaction to be aborted.</p> <p>
     *
     *  All cursors opened within the transaction must be closed
     *  before the transaction is aborted.</p> <p>
     *
     *  After DbTxn.abort has been called, regardless of its return,
     *  the {@link com.sleepycat.db.DbTxn DbTxn} handle may not be
     *  accessed again.</p>
     *
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     */
    public void abort() throws DbException {
        try {
            abort0();
        } finally {
            swigCPtr = 0;
        }
    }


    /**
     *  The DbTxn.commit method ends the transaction.</p> <p>
     *
     *  In the case of nested transactions, if the transaction is a
     *  parent transaction, committing the parent transaction causes
     *  all unresolved children of the parent to be committed. In the
     *  case of nested transactions, if the transaction is a child
     *  transaction, its locks are not released, but are acquired by
     *  its parent. Although the commit of the child transaction will
     *  succeed, the actual resolution of the child transaction is
     *  postponed until the parent transaction is committed or
     *  aborted; that is, if its parent transaction commits, it will
     *  be committed; and if its parent transaction aborts, it will be
     *  aborted.</p> <p>
     *
     *  All cursors opened within the transaction must be closed
     *  before the transaction is committed.</p> <p>
     *
     *  After DbTxn.commit has been called, regardless of its return,
     *  the {@link com.sleepycat.db.DbTxn DbTxn} handle may not be
     *  accessed again. If DbTxn.commit encounters an error, the
     *  transaction and all child transactions of the transaction are
     *  aborted.</p>
     *
     * @param  flags         must be set to 0 or one of the following
     *      values:
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_TXN_NOSYNC
     *        Db.DB_TXN_NOSYNC}<p>
     *
     *        Do not synchronously flush the log. This means the
     *        transaction will exhibit the ACI (atomicity,
     *        consistency, and isolation) properties, but not D
     *        (durability); that is, database integrity will be
     *        maintained, but it is possible that this transaction may
     *        be undone during recovery. <p>
     *
     *        This behavior may be set for a Berkeley DB environment
     *        using the {@link com.sleepycat.db.DbEnv#setFlags
     *        DbEnv.setFlags} method or for a single transaction using
     *        the {@link com.sleepycat.db.DbEnv#txnBegin
     *        DbEnv.txnBegin} method. Any value specified to this
     *        method overrides both of those settings.</p> </li>
     *      </ul>
     *
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_TXN_SYNC
     *        Db.DB_TXN_SYNC}<p>
     *
     *        Synchronously flush the log. This means the transaction
     *        will exhibit all of the ACID (atomicity, consistency,
     *        isolation, and durability) properties. <p>
     *
     *        This behavior is the default for Berkeley DB
     *        environments unless the {@link
     *        com.sleepycat.db.Db#DB_TXN_NOSYNC <code>Db.DB_TXN_NOSYNC</code>
     *        } flag was specified to the {@link
     *        com.sleepycat.db.DbEnv#setFlags DbEnv.setFlags} method.
     *        This behavior may also be set for a single transaction
     *        using the {@link com.sleepycat.db.DbEnv#txnBegin
     *        DbEnv.txnBegin} method. Any value specified to this
     *        method overrides both of those settings.</p> </li>
     *      </ul>
     *
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     */
    public void commit(int flags) throws DbException {
        try {
            commit0(flags);
        } finally {
            swigCPtr = 0;
        }
    }


    /**
     *  The DbTxn.discard method frees up all the per-process
     *  resources associated with the specified {@link
     *  com.sleepycat.db.DbTxn DbTxn} handle, neither committing nor
     *  aborting the transaction. This call may be used only after
     *  calls to {@link com.sleepycat.db.DbEnv#txnRecover
     *  DbEnv.txnRecover} when there are multiple global transaction
     *  managers recovering transactions in a single Berkeley DB
     *  environment. Any transactions returned by {@link
     *  com.sleepycat.db.DbEnv#txnRecover DbEnv.txnRecover} that are
     *  not handled by the current global transaction manager should
     *  be discarded using DbTxn.discard.</p> After DbTxn.discard has
     *  been called, regardless of its return, the {@link
     *  com.sleepycat.db.DbTxn DbTxn} handle may not be accessed
     *  again.</p>
     *
     * @param  flags                      currently unused, and must
     *      be set to 0.
     * @throws  IllegalArgumentException  The DbTxn.discard method
     *      will fail and throw a IllegalArgumentException exception
     *      if the transaction handle does not refer to a transaction
     *      that was recovered into a prepared but not yet completed
     *      state; or if an invalid flag value or parameter was
     *      specified.
     */
    public void discard(int flags) throws DbException {
        try {
            discard0(flags);
        } finally {
            swigCPtr = 0;
        }
    }


    // We override Object.equals because it is possible for
    // the Java API to create multiple DbTxns that reference
    // the same underlying object.  This can happen for example
    // during DbEnv.txn_recover().
    //
    public boolean equals(Object obj) {

        if (this == obj) {
            return true;
        }

        if (obj != null && (obj instanceof DbTxn)) {
            DbTxn that = (DbTxn) obj;
            return (this.swigCPtr == that.swigCPtr);
        }
        return false;
    }


    // We must override Object.hashCode whenever we override
    // Object.equals() to enforce the maxim that equal objects
    // have the same hashcode.
    //
    public int hashCode() {

        return ((int) swigCPtr ^ (int) (swigCPtr >> 32));
    }


    void abort0() {
        db_javaJNI.DbTxn_abort0(swigCPtr);
    }


    void commit0(int flags) {
        db_javaJNI.DbTxn_commit0(swigCPtr, flags);
    }


    void discard0(int flags) {
        db_javaJNI.DbTxn_discard0(swigCPtr, flags);
    }


    /**
     *  The DbTxn.id method returns the unique transaction id
     *  associated with the specified transaction. Locking calls made
     *  on behalf of this transaction should use the value returned
     *  from DbTxn.id as the locker parameter to the {@link
     *  com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} or {@link
     *  com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} calls.</p>
     *
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     * @return               The DbTxn.id method returns the unique
     *      transaction id associated with the specified transaction.
     */
    public int id() throws DbException {
        return db_javaJNI.DbTxn_id(swigCPtr);
    }


    /**
     *  The DbTxn.prepare method initiates the beginning of a
     *  two-phase commit.</p> <p>
     *
     *  In a distributed transaction environment, Berkeley DB can be
     *  used as a local transaction manager. In this case, the
     *  distributed transaction manager must send <i>prepare</i>
     *  messages to each local manager. The local manager must then
     *  issue a DbTxn.prepare and await its successful return before
     *  responding to the distributed transaction manager. Only after
     *  the distributed transaction manager receives successful
     *  responses from all of its <i>prepare</i> messages should it
     *  issue any <i>commit</i> messages.</p> <p>
     *
     *  In the case of nested transactions, preparing the parent
     *  causes all unresolved children of the parent transaction to be
     *  committed. Child transactions should never be explicitly
     *  prepared. Their fate will be resolved along with their
     *  parent's during global recovery.</p>
     *
     * @param  gid           specifies the global transaction ID by
     *      which this transaction will be known. This global
     *      transaction ID will be returned in calls to {@link
     *      com.sleepycat.db.DbEnv#txnRecover DbEnv.txnRecover},
     *      telling the application which global transactions must be
     *      resolved.
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_XIDDATASIZE
     *        Db.DB_XIDDATASIZE}<p>
     *
     *        must be sized at least <code>Db.DB_XIDDATASIZE</code>
     *        (currently 128) bytes; only the first <code>Db.DB_XIDDATASIZE</code>
     *        bytes are used. </li>
     *      </ul>
     *
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     */
    public void prepare(byte[] gid) throws DbException {
        db_javaJNI.DbTxn_prepare(swigCPtr, gid);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setTimeout(long,int)}
     */
    public void set_timeout(long timeout, int flags) throws DbException {
        setTimeout(timeout, flags);
    }


    /**
     *  The DbTxn.setTimeout method sets timeout values for locks or
     *  transactions for the specified transaction.</p> <p>
     *
     *  Timeouts are checked whenever a thread of control blocks on a
     *  lock or when deadlock detection is performed. (In the case of
     *  Db.DB_SET_LOCK_TIMEOUT, the lock is one requested explicitly
     *  through the Lock subsystem interfaces. In the case of
     *  Db.DB_SET_TXN_TIMEOUT, the lock is one requested on behalf of
     *  a transaction. In either case, it may be a lock requested by
     *  the database access methods underlying the application.) As
     *  timeouts are only checked when the lock request first blocks
     *  or when deadlock detection is performed, the accuracy of the
     *  timeout depends on how often deadlock detection is performed.
     *  </p> <p>
     *
     *  Timeout values may be specified for the database environment
     *  as a whole. See {@link com.sleepycat.db.DbEnv#setTimeout
     *  DbEnv.setTimeout} and for more information.</p> <p>
     *
     *  The DbTxn.setTimeout method configures operations performed on
     *  the underlying transaction, not only operations performed
     *  using the specified {@link com.sleepycat.db.DbTxn DbTxn}
     *  handle.</p> <p>
     *
     *  The DbTxn.setTimeout method may be called at any time during
     *  the life of the application.</p>
     *
     * @param  flags                      must be set to one of the
     *      following values:
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_SET_LOCK_TIMEOUT
     *        Db.DB_SET_LOCK_TIMEOUT}<p>
     *
     *        Set the timeout value for locks in this transaction.
     *        </li>
     *      </ul>
     *
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_SET_TXN_TIMEOUT
     *        Db.DB_SET_TXN_TIMEOUT}<p>
     *
     *        Set the timeout value for this transaction. </li>
     *      </ul>
     *
     * @param  timeout                    specified as an unsigned
     *      32-bit number of microseconds, limiting the maximum
     *      timeout to roughly 71 minutes. A value of 0 disables
     *      timeouts for the transaction.
     * @throws  IllegalArgumentException  The DbTxn.setTimeout method
     *      will fail and throw a IllegalArgumentException exception
     *      if an invalid flag value or parameter was specified.
     */
    public void setTimeout(long timeout, int flags) throws DbException {
        db_javaJNI.DbTxn_set_timeout(swigCPtr, timeout, flags);
    }

}
