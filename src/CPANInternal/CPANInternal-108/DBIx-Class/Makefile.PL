use inc::Module::Install 0.93;
use strict;
use warnings;
use POSIX ();

use 5.008001;

use FindBin;
use lib "$FindBin::Bin/lib";

# adjust ENV for $AUTHOR system() calls
use Config;
$ENV{PERL5LIB} = join ($Config{path_sep}, @INC);


###
### DO NOT ADD OPTIONAL DEPENDENCIES HERE, EVEN AS recommends()
### All of them should go to DBIx::Class::Optional::Dependencies
###


name     'DBIx-Class';
perl_version '5.008001';
all_from 'lib/DBIx/Class.pm';

my $build_requires = {
  'DBD::SQLite'              => '1.25',
};

my $test_requires = {
  'File::Temp'               => '0.22',
  'Test::Builder'            => '0.33',
  'Test::Exception'          => '0',
  'Test::More'               => '0.92',
  'Test::Warn'               => '0.21',
};

my $runtime_requires = {
  'Carp::Clan'               => '6.0',
  'Class::Accessor::Grouped' => '0.09002',
  'Class::C3::Componentised' => '1.0005',
  'Class::Inspector'         => '1.24',
  'Data::Page'               => '2.00',
  'DBI'                      => '1.609',
  'MRO::Compat'              => '0.09',
  'Module::Find'             => '0.06',
  'Path::Class'              => '0.18',
  'SQL::Abstract'            => '1.61',
  'SQL::Abstract::Limit'     => '0.13',
  'Sub::Name'                => '0.04',
  'Data::Dumper::Concise'    => '1.000',
  'Scope::Guard'             => '0.03',
  'Context::Preserve'        => '0.01',
};

# this is so we can order requires alphabetically
# copies are needed for author requires injection
my $reqs = {
  build_requires => { %$build_requires },
  requires => { %$runtime_requires },
  test_requires => { %$test_requires },
};


# require extra modules for testing if we're in a checkout
if ($Module::Install::AUTHOR) {
  warn <<'EOW';
******************************************************************************
******************************************************************************
***                                                                        ***
*** AUTHOR MODE: all optional test dependencies converted to hard requires ***
***                                                                        ***
******************************************************************************
******************************************************************************

EOW

  require DBIx::Class::Optional::Dependencies;
  $reqs->{test_requires} = {
    %{$reqs->{test_requires}},
    %{DBIx::Class::Optional::Dependencies->_all_optional_requirements},
  };
}

# compose final req list, for alphabetical ordering
my %final_req;
for my $rtype (keys %$reqs) {
  for my $mod (keys %{$reqs->{$rtype}} ) {

    # sanity check req duplications
    if ($final_req{$mod}) {
      die "$mod specified as both a '$rtype' and a '$final_req{$mod}[0]'\n";
    }

    $final_req{$mod} = [ $rtype, $reqs->{$rtype}{$mod}||0 ],
  }
}

# actual require
for my $mod (sort keys %final_req) {
  my ($rtype, $ver) = @{$final_req{$mod}};
  no strict 'refs';
  $rtype->($mod, $ver);
}

auto_install();

# re-create various autogenerated documentation bits
if ($Module::Install::AUTHOR) {

  print "Regenerating README\n";
  system('pod2text lib/DBIx/Class.pm > README');

  if (-f 'MANIFEST') {
    print "Removing MANIFEST\n";
    unlink 'MANIFEST';
  }

  print "Regenerating Optional/Dependencies.pod\n";
  require DBIx::Class::Optional::Dependencies;
  DBIx::Class::Optional::Dependencies->_gen_pod;

  # FIXME Disabled due to unsolved issues, ask theorbtwo
  #  require Module::Install::Pod::Inherit;
  #  PodInherit();
}

tests_recursive (qw|
    t
|);

install_script (qw|
    script/dbicadmin
|);


### Mangle makefile - read the comments for more info
#
postamble <<"EOP";

# This will add an extra dep-spec for the distdir target,
# which `make` will fold together in a first-come first-serve
# fashion. What we do here is essentially adding extra
# commands to execute once the distdir is assembled (via
# create_distdir), but before control is returned to a higher
# calling rule.
distdir : dbicadmin_pod_inject

# The pod self-injection code is in fact a hidden option in
# dbicadmin itself
dbicadmin_pod_inject :
\tcd \$(DISTVNAME) && \$(ABSPERL) -Ilib script/dbicadmin --selfinject-pod

# Regenerate manifest before running create_distdir.
create_distdir : manifest

EOP



resources 'IRC'         => 'irc://irc.perl.org/#dbix-class';
resources 'license'     => 'http://dev.perl.org/licenses/';
resources 'repository'  => 'http://dev.catalyst.perl.org/repos/bast/DBIx-Class/';
resources 'MailingList' => 'http://lists.scsys.co.uk/cgi-bin/mailman/listinfo/dbix-class';

# Deprecated/internal modules need no exposure
no_index directory => $_ for (qw|
  lib/DBIx/Class/Admin
  lib/DBIx/Class/SQLAHacks
  lib/DBIx/Class/PK/Auto
  lib/DBIx/Class/CDBICompat
|);
no_index package => $_ for (qw/
  DBIx::Class::SQLAHacks DBIx::Class::Storage::DBIHacks
/);


WriteAll();


# Re-write META.yml to _exclude_ all forced requires (we do not want to ship this)
if ($Module::Install::AUTHOR) {

  # FIXME test_requires is not yet part of META
  my %original_build_requires = ( %$build_requires, %$test_requires );

  print "Regenerating META with author requires excluded\n";
  Meta->{values}{build_requires} = [ grep
    { exists $original_build_requires{$_->[0]} }
   ( @{Meta->{values}{build_requires}} )
  ];

  Meta->write;
}
