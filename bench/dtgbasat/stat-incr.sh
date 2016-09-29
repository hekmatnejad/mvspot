#!/bin/sh

tmpdir="/work/aga/tmp-$$"
mkdir $tmpdir
export TMPDIR="$tmpdir"

ltlfilt=../../bin/ltlfilt
ltl2tgba=../../bin/ltl2tgba
dstar2tgba=../../bin/dstar2tgba
timeout='timeout -sKILL 15m'
stats=--stats="%s, %e, %t, %a, %c, %d, %p, %r"
empty='-, -, -, -, -, -, -, -'
tomany='!, !, !, !, !, !, !, !'
dbamin=${DBA_MINIMIZER-/home/adl/projs/dbaminimizer/minimize.py}

get_stats()
{
  type=$1
  shift
  SPOT_SATLOG=$n.$type.satlog $timeout "$@" "$stats" > stdin.$$ 2>stderr.$$
  if grep -q 'INT_MAX' stderr.$$; then
    # Too many SAT-clause?
    echo "$tomany"
  else
    tmp=`cat stdin.$$`
    echo ${tmp:-$empty}
  fi
  rm -f stdin.$$ stderr.$$
}

get_dbamin_stats()
{
  tmp=`./rundbamin.pl $timeout $dbamin "$@"`
  mye='-, -'
  echo ${tmp:-$mye}
}

n=$1
f=$2
type=$3
accmax=$4

# mindba1: cnf_file with glucose, mindba2: cnf_file with picosat
# mindba3: libpicosat, mindba4: libpicosat with incremental solving
case $type in
  *WDBA*)
    echo "$f, $accmax, $type..." 1>&2
    input=`get_stats BA $ltl2tgba "$f" -BD`
    dba=$input
    # mindba[1|2|3]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1..." 1>&2
    mindba1=`get_stats DBA1 $ltl2tgba "$f" -BD -x 'sat-minimize=4,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2..." 1>&2
    mindba2=`get_stats DBA2 $ltl2tgba "$f" -BD -x 'sat-minimize=4,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3..." 1>&2
    mindba3=`get_stats DBA3 $ltl2tgba "$f" -BD -x 'sat-minimize=4,sat-incr=3'`
    # mindtgba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1..." 1>&2
    mindtgba1=`get_stats DTGBA1 $ltl2tgba "$f" -D -x 'sat-minimize=4,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2..." 1>&2
    mindtgba2=`get_stats DTGBA2 $ltl2tgba "$f" -D -x 'sat-minimize=4,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3..." 1>&2
    mindtgba3=`get_stats DTGBA3 $ltl2tgba "$f" -D -x 'sat-minimize=4,sat-incr=3'`
    # mindtba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1..." 1>&2
    mindtba1=`get_stats DTBA1 $ltl2tgba "$f" -D -x 'sat-minimize=4,sat-acc=1,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2..." 1>&2
    mindtba2=`get_stats DTBA2 $ltl2tgba "$f" -D -x 'sat-minimize=4,sat-acc=1,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2, $mindtba2, minDTBA3..." 1>&2
    mindtba3=`get_stats DTBA3 $ltl2tgba "$f" -D -x 'sat-minimize=4,sat-acc=1,sat-incr=3'`
    # dbaminimizer
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2, $mindtba2, minDTBA3, $mindtba3, dbaminimizer..." 1>&2
    $ltlfilt --remove-wm -f "$f" -l |
    ltl2dstar --ltl2nba=spin:$ltl2tgba@-Ds - dra.$$
    dbamin=`get_dbamin_stats dra.$$`
    dra=`sed -n 's/States: \(.*\)/\1/p' dra.$$`
    rm dra.$$
    ;;
  *TCONG*|*trad*)  # Not in WDBA
    echo "$f, $accmax, $type..." 1>&2
    input=`get_stats TBA $ltl2tgba "$f" -D -x '!wdba-minimize,tba-det'`
    echo "$f, $accmax, $type, $input, DBA, ..." 1>&2
    dba=`get_stats BA $ltl2tgba "$f" -BD -x '!wdba-minimize,tba-det'`
    # mindba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1..." 1>&2
    mindba1=`get_stats DBA1 $ltl2tgba "$f" -BD -x '!wdba-minimize,sat-minimize=4,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2..." 1>&2
    mindba2=`get_stats DBA2 $ltl2tgba "$f" -BD -x '!wdba-minimize,sat-minimize=4,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3..." 1>&2
    mindba3=`get_stats DBA3 $ltl2tgba "$f" -BD -x '!wdba-minimize,sat-minimize=4,sat-incr=3'`
    # mindtgba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1..." 1>&2
    mindtgba1=`get_stats DTGBA1 $ltl2tgba "$f" -D -x '!wdba-minimize,sat-minimize=4,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2..." 1>&2
    mindtgba2=`get_stats DTGBA2 $ltl2tgba "$f" -D -x '!wdba-minimize,sat-minimize=4,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3..." 1>&2
    mindtgba3=`get_stats DTGBA3 $ltl2tgba "$f" -D -x '!wdba-minimize,sat-minimize=4,sat-incr=3'`
    # mindtba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1..." 1>&2
    mindtba1=`get_stats DTBA1 $ltl2tgba "$f" -D -x '!wdba-minimize,sat-minimize=4,sat-acc=1,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2..." 1>&2
    mindtba2=`get_stats DTBA2 $ltl2tgba "$f" -D -x '!wdba-minimize,sat-minimize=4,sat-acc=1,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2, $mindtba2, minDTBA3..." 1>&2
    mindtba3=`get_stats DTBA3 $ltl2tgba "$f" -D -x '!wdba-minimize,sat-minimize=4,sat-acc=1,sat-incr=3'`
    # dbaminimizer
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2, $mindtba2, minDTBA3, $mindtba3, dbaminimizer..." 1>&2
    case $type in
      *TCONG*)   dbamin="n/a, n/a"  dra="n/a";;
      *trad*)
        $ltlfilt --remove-wm -f "$f" -l |
        ltl2dstar --ltl2nba=spin:$ltl2tgba@-Ds - dra.$$
        dbamin=`get_dbamin_stats dra.$$`
        dra=`sed -n 's/States: \(.*\)/\1/p' dra.$$`
        rm dra.$$
        ;;
    esac
    ;;
  *DRA*)
    echo "$f, $accmax, $type... " 1>&2
    $ltlfilt --remove-wm -f "$f" -l |
    ltl2dstar --ltl2nba=spin:$ltl2tgba@-Ds - dra.$$
    input=`get_stats TBA $dstar2tgba dra.$$ -D -x '!wdba-minimize'`
    echo "$f, $accmax, $type, $input, DBA, ... " 1>&2
    dba=`get_stats BA $dstar2tgba dra.$$ -BD -x '!wdba-minimize'`
    # mindba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1... " 1>&2
    mindba1=`get_stats DBA1 $dstar2tgba dra.$$ -BD -x '!wdba-minimize,sat-minimize=4,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2..." 1>&2
    mindba2=`get_stats DBA2 $dstar2tgba dra.$$ -BD -x '!wdba-minimize,sat-minimize=4,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3..." 1>&2
    mindba3=`get_stats DBA3 $dstar2tgba dra.$$ -BD -x '!wdba-minimize,sat-minimize=4,sat-incr=3'`
    # mindtgba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1..." 1>&2
    mindtgba1=`get_stats DTGBA1 $dstar2tgba dra.$$ -D -x '!wdba-minimize,sat-minimize=4,sat-incr=1,sat-acc='$accmax`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2..." 1>&2
    mindtgba2=`get_stats DTGBA2 $dstar2tgba dra.$$ -D -x '!wdba-minimize,sat-minimize=4,sat-acc='$accmax`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3..." 1>&2
    mindtgba3=`get_stats DTGBA3 $dstar2tgba dra.$$ -D -x '!wdba-minimize,sat-minimize=4,sat-incr=3,sat-acc='$accmax`
    # mindtba[1|2|3|4]
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1..." 1>&2
    mindtba1=`get_stats DTBA1 $dstar2tgba dra.$$ -D -x '!wdba-minimize,sat-minimize=4,sat-acc=1,sat-incr=1'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2..." 1>&2
    mindtba2=`get_stats DTBA2 $dstar2tgba dra.$$ -D -x '!wdba-minimize,sat-minimize=4,sat-acc=1,sat-incr=2'`
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2, $mindtba2, minDTBA3..." 1>&2
    mindtba3=`get_stats DTBA3 $dstar2tgba dra.$$ -D -x '!wdba-minimize,sat-minimize=4,sat-acc=1,sat-incr=3'`
    # dbaminimizer
    echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDBA2, $mindba2, minDBA3, $mindba3, minDTGBA1, $mindtgba1, minDTGBA2, $mindtgba2, minDTGBA3, $mindtgba3, minDTBA1, $mindtba1, minDTBA2, $mindtba2, minDTBA3, $mindtba3, dbaminimizer..." 1>&2
    dbamin=`get_dbamin_stats dra.$$`
    dra=`sed -n 's/States: \(.*\)/\1/p' dra.$$`
    rm -f dra.$$
    ;;
  *not*)
    exit 0
    ;;
  *)
    echo "SHOULD NOT HAPPEND"
    exit 2
    ;;
esac
echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDTGBA1, $mindtgba1, minDTBA1, $mindtba1, minDBA2, $mindba2, minDTGBA2, $mindtgba2, minDTBA2, $mindtba2, minDBA3, $mindba3, minDTGBA3, $mindtgba3, minDTBA3, $mindtba3, dbaminimizer, $dbamin, DRA, $dra, $n" 1>&2
echo "$f, $accmax, $type, $input, DBA, $dba, minDBA1, $mindba1, minDTGBA1, $mindtgba1, minDTBA1, $mindtba1, minDBA2, $mindba2, minDTGBA2, $mindtgba2, minDTBA2, $mindtba2, minDBA3, $mindba3, minDTGBA3, $mindtgba3, minDTBA3, $mindtba3, dbaminimizer, $dbamin, DRA, $dra, $n"

echo $f > "$tmpdir/formula"
rm -rf $tmpdir
