###########################################################################
#
# This file is auto-generated by the Perl DateTime Suite time locale
# generator (0.02).  This code generator comes with the
# DateTime::Locale distribution in the tools/ directory, and is called
# generate_from_icu.
#
# This file as generated from the ICU XML locale data.  See the
# LICENSE.icu file included in this distribution for license details.
#
# This file was generated from the source file te.xml.
# The source file version number was 1.2, generated on
# 2004-08-27.
#
# Do not edit this file directly.
#
###########################################################################

package DateTime::Locale::te;

use strict;

BEGIN
{
    if ( $] >= 5.006 )
    {
        require utf8; utf8->import;
    }
}

use DateTime::Locale::root;

@DateTime::Locale::te::ISA = qw(DateTime::Locale::root);

my @day_names = (
"సోమవారం",
"మంగళవారం",
"బుధవారం",
"గురువారం",
"శుక్రవారం",
"శనివారం",
"ఆదివారం",
);

my @day_abbreviations = (
"సోమ",
"మంగళ",
"బుధ",
"గురు",
"శుక్ర",
"శని",
"ఆది",
);

my @month_names = (
"జనవరి",
"ఫిబ్రవరి",
"మార్చి",
"ఏప్రిల్",
"మే",
"జూన్",
"జూలై",
"ఆగస్టు",
"సెప్టెంబర్",
"అక్టోబర్",
"నవంబర్",
"డిసెంబర్",
);

my @month_abbreviations = (
"జనవరి",
"ఫిబ్రవరి",
"మార్చి",
"ఏప్రిల్",
"మే",
"జూన్",
"జూలై",
"ఆగస్టు",
"సెప్టెంబర్",
"అక్టోబర్",
"నవంబర్",
"డిసెంబర్",
);

my @am_pms = (
"పూర్వాహ్న",
"అపరాహ్న",
);



sub day_names                      { \@day_names }
sub day_abbreviations              { \@day_abbreviations }
sub month_names                    { \@month_names }
sub month_abbreviations            { \@month_abbreviations }
sub am_pms                         { \@am_pms }



1;

