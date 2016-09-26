#!/usr/bin/perl -w

use strict;

my $num = 0;
my @v = ();
while (<>)
{
    # G(a | Fb), 1, trad, 2, 4, 8, 1, 1, 1, 1, 0.00165238, DBA, 2, 4, 8, 1, 1, 1, 1, 0.00197852, minDBA, 2, 4, 8, 1, 1, 1, 1, 0.00821457, minDTGBA, 2, 4, 8, 1, 1, 1, 1, 0.0081701
    chomp;
    next if /.*realizable.*/;
    $v[$num] = [split /,/];
#    print $v[$num]->[48], " ",  $#{$v[$num]}, "\n";
#    if ($#{$v[$num]} != 49)
#    {
#	pop $v[$num];
#	push $v[$num], '-', '-';
#   }
#    print $v[$num]->[48], " ",  $#{$v[$num]}, "\n";

    ++$num;
}

sub dratcong($$)
{
    my ($a, $b) = @_;
    return 0 if ($a =~ /.*CONG/ && $b =~ /.*CONG/);
    return $a cmp $b;
}

sub mycmp()
{
    my $v = dratcong($b->[2], $a->[2]);
    return $v if $v;
    my $n1 = lc $a->[0];
    $n1 =~ s/\W//g;
    my $n2 = lc $b->[0];
    $n2 =~ s/\W//g;
    return $n1 cmp $n2 || $a->[0] cmp $b->[0] || $b->[2] cmp $a->[2];;
}

my @w = sort mycmp @v;

print "\\documentclass{standalone}\n
\\usepackage{amsmath}
\\usepackage{colortbl}
\\definecolor{mygray}{gray}{0.75} % 1 = white, 0 = black
\\definecolor{lightgray}{gray}{0.7} % 1 = white, 0 = black
\\def\\E{\\cellcolor{mygray}}
\\def\\P{\\cellcolor{red}}
\\def\\PP{\\cellcolor{yellow}}
\\def\\F{\\mathsf{F}} % in future
\\def\\G{\\mathsf{G}} % globally
\\def\\X{\\mathsf{X}} % next
\\DeclareMathOperator{\\W}{\\mathbin{\\mathsf{W}}} % weak until
\\DeclareMathOperator{\\M}{\\mathbin{\\mathsf{M}}} % strong release
\\DeclareMathOperator{\\U}{\\mathbin{\\mathsf{U}}} % until
\\DeclareMathOperator{\\R}{\\mathbin{\\mathsf{R}}} % release
";

print "\\begin{document}\n";
print "\\begin{tabular}{lrcr|r|rrrr|rrr|rr|rrr|rrr|rrrr|rrr|rrr|rrrr|rrr|rrr|rrrr|rrr|rrr|rrrr}";
print "\\multicolumn{54}{l}{Column \\textbf{type} shows how the initial det. aut. was obtained: T = translation produces DTGBA; W = WDBA minimization works; P = powerset construction transforms TBA to DTBA; R = DRA to DBA.}\\\\\n";
print "\\multicolumn{54}{l}{Column \\textbf{C.} tells whether the output automaton is complete: rejecting sink states are always omitted (add 1 state when C=0 if you want the size of the complete automaton).}\\\\\n";
print "\\multicolumn{14}{|c}{}& \\multicolumn{10}{|c}{glu. cnf}& \\multicolumn{10}{|c}{pic. cnf}& \\multicolumn{10}{|c}{pic. lib}& \\multicolumn{10}{|c}{pic. lib incremental}\\\\\n";
print "&&&&DRA&\\multicolumn{4}{|c}{DTGBA} & \\multicolumn{3}{|c}{DBA} &  \\multicolumn{2}{|c}{DBA\\footnotesize minimizer}&  \\multicolumn{3}{|c}{min DBA} & \\multicolumn{3}{|c}{minDTBA} & \\multicolumn{4}{|c}{minDTGBA} & \\multicolumn{3}{|c}{min DBA} & \\multicolumn{3}{|c}{minDTBA} & \\multicolumn{4}{|c}{minDTGBA} &  \\multicolumn{3}{|c}{min DBA} & \\multicolumn{3}{|c}{minDTBA} & \\multicolumn{4}{|c}{minDTGBA} &  \\multicolumn{3}{|c}{min DBA} & \\multicolumn{3}{|c}{minDTBA} & \\multicolumn{4}{|c}{minDTGBA}\\\\\n";
print "formula& \$m\$ & type & C. & st.& st. & tr. & acc. & time & st. & tr. & time & st. & time & st. & tr. & time & st. & tr. & time & st. & tr. & acc. & time & st. & tr. & time & st. & tr. & time & st. & tr. & acc. & time & st. & tr. & time & st. & tr. & time & st. & tr. & acc. & time & st. & tr. & time & st. & tr. & time & st. & tr. & acc. & time \\\\\n";

sub nonnull($)
{
    return 1 if $_[0] == 0;
    return $_[0];
}

sub getlastsuccesful($$)
{
    my ($n,$type) = @_;
    open LOG, "<$n.$type.satlog" or return "";
    my $min = "";
    while (my $line = <LOG>)
    {
	my @f = split(/,/, $line);
	$min = $f[1] if $f[1] ne '';
    }
    $min = ", \$\\le\$$min" if $min ne "";
    return $min;
}


my $lasttype = '';
my $linenum = 0;
foreach my $tab (@w)
{
    sub val($)
    {
	my ($n) = @_;
	my $v = $tab->[$n];
	return 0+'inf' if !defined($v) || $v =~ /\s*-\s*/;
	return $v;
    }

    if (dratcong($lasttype, $tab->[2]))
    {
	print "\\hline\n";
	$linenum = 0;
    }
    $lasttype = $tab->[2];
    if ($linenum++ % 4 == 0)
    {
	print "\\arrayrulecolor{lightgray}\\hline\\arrayrulecolor{black}";
    }

    my $n = $tab->[133];
    my $f = $tab->[0];
    $f =~ s/\&/\\land /g;
    $f =~ s/\|/\\lor /g;
    $f =~ s/!/\\bar /g;
    $f =~ s/<->/\\leftrightarrow /g;
    $f =~ s/->/\\rightarrow /g;
    $f =~ s/[XRWMGFU]/\\$& /g;
    print "\$$f\$\t& ";                             # formula
    print "$tab->[1] & ";                           # m
    if ($tab->[2] =~ /trad/) { print "T & "; }      # type
    elsif ($tab->[2] =~ /TCONG/) { print "P & "; }
    elsif ($tab->[2] =~ /DRA/) { print "R & "; }
    elsif ($tab->[2] =~ /WDBA/) { print "W & "; }
    else { print "$tab->[2]& "; }
    # If one of the automata is not deterministic highlight the "Complete" column.
    print "{\\P}" if val(8) == 0 || val(17) == 0 || val(26) == 0 || val(35) == 0  || val(44) == 0 || val(53) == 0 || val(62) == 0 || val(71) == 0 || val(80) == 0 || val(89) == 0 || val(98) == 0 || val(10) == 0 || val(107) == 0 || val(116) == 0 || val(125) == 0;
    print "$tab->[9] & ";

    if ($tab->[132] =~ m:\s*n/a\s*:) #  DRA
    {
	print "&";
	$tab->[132] = 0+'inf';
    }
    else
    {
	# Remove sink state if not complete.
	my $st = $tab->[132] - 1 + $tab->[9] || 1;
	print "$st & ";
    }

    print "$tab->[3] & "; # states
    print "$tab->[5] & "; # transitions
    print "$tab->[6] & "; # acc
    printf("%.2f &", $tab->[10]);

    if ($tab->[12] =~ /\s*-\s*/)  # DBA
    {
	print "- & - & - &";
	$tab->[12] = 0+'inf';
    }
    elsif ($tab->[12] =~ /\s*!\s*/)  # DBA
    {
	print "! & ! & ! &";
	$tab->[12] = 0+'inf';
    }
    else
    {
	print "$tab->[12] & "; # states
	print "$tab->[14] & "; # transitions
	printf("%.2f &", $tab->[19]);
    }

    if ($tab->[129] =~ /\s*-\s*/) #  DBAminimizer
    {
	print "\\multicolumn{2}{c|}{(killed)}&";
	$tab->[129] = 0+'inf';
    }
    elsif ($tab->[129] =~ m:\s*n/a\s*:) #  DBAminimizer
    {
	print " & &";
	$tab->[129] = 0+'inf';
    }
    else
    {
	# Remove sink state if not complete.
	my $st = $tab->[129] - 1 + $tab->[9] || 1;
	print "{\\E}" if ($st < $tab->[12]);
	print "{\\P}" if ($st > $tab->[12]);
	print "$st & "; # states
	printf("%.2f &", $tab->[130]);
    }

    #################||    glu. cnf    ||####################

    if ($tab->[21] =~ /\s*-\s*/) #  minDBA 1
    {
	my $s = getlastsuccesful($n, "DBA1");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[21] = 0+'inf';
    }
    elsif ($tab->[21] =~ /\s*!\s*/) #  minDBA 1
    {
	my $s = getlastsuccesful($n, "DBA1");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[21] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[21] < $tab->[12]);
	my $st = $tab->[129] - 1 + $tab->[9] || 1;
	print "{\\P}" if ($tab->[21] > $tab->[12]) or ($tab->[21] > $st);
	print "$tab->[21] & "; # states
	print "$tab->[23] & "; # transitions
	printf("%.2f &", $tab->[28]);
    }

    if ($tab->[39] =~ /\s*-\s*/) # min DTBA 1
    {
	my $s = getlastsuccesful($n, "DTBA1");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[39] = 0+'inf';
    }
    elsif ($tab->[39] =~ /\s*!\s*/) # min DTBA 1
    {
	my $s = getlastsuccesful($n, "DTBA1");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[39] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[39] < $tab->[3]);
	print "{\\P}" if ($tab->[39] > $tab->[3] * nonnull($tab->[6])) or ($tab->[39] > $tab->[12]) or ($tab->[39] > $tab->[21]);
	print "\\textbf" if ($tab->[39] < $tab->[21]);
	print "{$tab->[39]} & "; # states
	print "$tab->[41] & "; # transitions
	printf("%.2f &", $tab->[46]);
    }

    if ($tab->[30] =~ /\s*-\s*/)   # minDTGBA 1
    {
	my $s = getlastsuccesful($n, "DTGBA1");
	print "\\multicolumn{4}{c}{(killed$s)}&";
	$tab->[30] = 0+'inf';
    }
    elsif ($tab->[30] =~ /\s*!\s*/)   # minDTGBA 1
    {
	my $s = getlastsuccesful($n, "DTGBA1");
	print "\\multicolumn{4}{c}{(intmax$s)}&";
	$tab->[30] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[30] < $tab->[3]);
	print "{\\P}" if ($tab->[30] > $tab->[3]) or ($tab->[30] > $tab->[12]) or ($tab->[30] > $tab->[21]) or ($tab->[30] > $tab->[39]);
	print "{\\PP}" if ($tab->[21] ne 'inf' && $tab->[30] * ($tab->[33] + 1) < $tab->[21]);
	print "\\textbf" if ($tab->[30] < $tab->[39]);
	print "{$tab->[30]} & "; # states
	print "$tab->[32] & "; # transitions
	print "\\textbf" if ($tab->[33] > $tab->[6]);
	print "{$tab->[33]} & "; # acc
	printf("%.2f &", $tab->[37]);
    }

    #################||    pic. cnf    ||####################

    if ($tab->[48] =~ /\s*-\s*/) #  minDBA 2
    {
	my $s = getlastsuccesful($n, "DBA2");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[48] = 0+'inf';
    }
    elsif ($tab->[48] =~ /\s*!\s*/) #  minDBA 2
    {
	my $s = getlastsuccesful($n, "DBA2");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[48] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[48] < $tab->[12]);
	my $st = $tab->[129] - 1 + $tab->[9] || 1;
	print "{\\P}" if ($tab->[48] > $tab->[12]) or ($tab->[48] > $st);
	print "$tab->[48] & "; # states
	print "$tab->[50] & "; # transitions
	printf("%.2f &", $tab->[55]);
    }

    if ($tab->[66] =~ /\s*-\s*/) # min DTBA 2
    {
	my $s = getlastsuccesful($n, "DTBA2");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[66] = 0+'inf';
    }
    elsif ($tab->[66] =~ /\s*!\s*/) # min DTBA 2
    {
	my $s = getlastsuccesful($n, "DTBA2");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[66] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[66] < $tab->[3]);
	print "{\\P}" if ($tab->[66] > $tab->[3] * nonnull($tab->[6])) or ($tab->[66] > $tab->[12]) or ($tab->[66] > $tab->[48]);
	print "\\textbf" if ($tab->[66] < $tab->[21]);
	print "{$tab->[66]} & "; # states
	print "$tab->[68] & "; # transitions
	printf("%.2f &", $tab->[73]);
    }

    if ($tab->[57] =~ /\s*-\s*/)   # minDTGBA 2
    {
	my $s = getlastsuccesful($n, "DTGBA2");
	print "\\multicolumn{4}{c}{(killed$s)}&";
	$tab->[57] = 0+'inf';
    }
    elsif ($tab->[57] =~ /\s*!\s*/)   # minDTGBA 2
    {
	my $s = getlastsuccesful($n, "DTGBA2");
	print "\\multicolumn{4}{c}{(intmax$s)}&";
	$tab->[57] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[57] < $tab->[3]);
	print "{\\P}" if ($tab->[57] > $tab->[3]) or ($tab->[57] > $tab->[12]) or ($tab->[57] > $tab->[48]) or ($tab->[57] > $tab->[66]);
	print "{\\PP}" if ($tab->[48] ne 'inf' && $tab->[57] * ($tab->[60] + 1) < $tab->[48]);
	print "\\textbf" if ($tab->[57] < $tab->[66]);
	print "{$tab->[57]} & "; # states
	print "$tab->[59] & "; # transitions
	print "\\textbf" if ($tab->[60] > $tab->[6]);
	print "{$tab->[60]} & "; # acc
	printf("%.2f &", $tab->[64]);
    }

    #################||    pic. lib    ||####################

    if ($tab->[75] =~ /\s*-\s*/) #  minDBA 3
    {
	my $s = getlastsuccesful($n, "DBA3");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[75] = 0+'inf';
    }
    elsif ($tab->[75] =~ /\s*!\s*/) #  minDBA 3
    {
	my $s = getlastsuccesful($n, "DBA3");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[75] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[75] < $tab->[12]);
	my $st = $tab->[129] - 1 + $tab->[9] || 1;
	print "{\\P}" if ($tab->[75] > $tab->[12]) or ($tab->[75] > $st);
	print "$tab->[75] & "; # states
	print "$tab->[77] & "; # transitions
	printf("%.2f &", $tab->[82]);
    }

    if ($tab->[93] =~ /\s*-\s*/) # min DTBA 3
    {
	my $s = getlastsuccesful($n, "DTBA3");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[93] = 0+'inf';
    }
    elsif ($tab->[93] =~ /\s*!\s*/) # min DTBA 3
    {
	my $s = getlastsuccesful($n, "DTBA3");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[93] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[93] < $tab->[3]);
	print "{\\P}" if ($tab->[93] > $tab->[3] * nonnull($tab->[6])) or ($tab->[93] > $tab->[12]) or ($tab->[93] > $tab->[75]);
	print "\\textbf" if ($tab->[93] < $tab->[21]);
	print "{$tab->[93]} & "; # states
	print "$tab->[95] & "; # transitions
	printf("%.2f &", $tab->[100]);
    }

    if ($tab->[84] =~ /\s*-\s*/)   # minDTGBA 3
    {
	my $s = getlastsuccesful($n, "DTGBA3");
	print "\\multicolumn{4}{c}{(killed$s)}&";
	$tab->[84] = 0+'inf';
    }
    elsif ($tab->[84] =~ /\s*!\s*/)   # minDTGBA 3
    {
	my $s = getlastsuccesful($n, "DTGBA3");
	print "\\multicolumn{4}{c}{(intmax$s)}&";
	$tab->[84] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[84] < $tab->[3]);
	print "{\\P}" if ($tab->[84] > $tab->[3]) or ($tab->[84] > $tab->[12]) or ($tab->[84] > $tab->[75]) or ($tab->[84] > $tab->[93]);
	print "{\\PP}" if ($tab->[75] ne 'inf' && $tab->[84] * ($tab->[87] + 1) < $tab->[75]);
	print "\\textbf" if ($tab->[84] < $tab->[93]);
	print "{$tab->[84]} & "; # states
	print "$tab->[86] & "; # transitions
	print "\\textbf" if ($tab->[87] > $tab->[6]);
	print "{$tab->[87]} & "; # acc
	printf("%.2f &", $tab->[91]);
    }

    #################||    pic. lib incr   ||####################

    if ($tab->[102] =~ /\s*-\s*/) #  minDBA 4
    {
	my $s = getlastsuccesful($n, "DBA4");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[102] = 0+'inf';
    }
    elsif ($tab->[102] =~ /\s*!\s*/) #  minDBA 4
    {
	my $s = getlastsuccesful($n, "DBA4");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[102] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[102] < $tab->[12]);
	my $st = $tab->[129] - 1 + $tab->[9] || 1;
	print "{\\P}" if ($tab->[102] > $tab->[12]) or ($tab->[102] > $st);
	print "$tab->[102] & "; # states
	print "$tab->[104] & "; # transitions
	printf("%.2f &", $tab->[109]);
    }

    if ($tab->[120] =~ /\s*-\s*/) # min DTBA 4
    {
	my $s = getlastsuccesful($n, "DTBA4");
	print "\\multicolumn{3}{c|}{(killed$s)}&";
	$tab->[120] = 0+'inf';
    }
    elsif ($tab->[120] =~ /\s*!\s*/) # min DTBA 4
    {
	my $s = getlastsuccesful($n, "DTBA4");
	print "\\multicolumn{3}{c|}{(intmax$s)}&";
	$tab->[120] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[120] < $tab->[3]);
	print "{\\P}" if ($tab->[120] > $tab->[3] * nonnull($tab->[6])) or ($tab->[120] > $tab->[12]) or ($tab->[120] > $tab->[102]);
	print "\\textbf" if ($tab->[120] < $tab->[21]);
	print "{$tab->[120]} & "; # states
	print "$tab->[122] & "; # transitions
	printf("%.2f &", $tab->[127]);
    }

    if ($tab->[111] =~ /\s*-\s*/)   # minDTGBA 4
    {
	my $s = getlastsuccesful($n, "DTGBA4");
	print "\\multicolumn{4}{c}{(killed$s)}";
	$tab->[111] = 0+'inf';
    }
    elsif ($tab->[111] =~ /\s*!\s*/)   # minDTGBA 4
    {
	my $s = getlastsuccesful($n, "DTGBA4");
	print "\\multicolumn{4}{c}{(intmax$s)}";
	$tab->[111] = 0+'inf';
    }
    else
    {
	print "{\\E}" if ($tab->[111] < $tab->[3]);
	print "{\\P}" if ($tab->[111] > $tab->[3]) or ($tab->[111] > $tab->[12]) or ($tab->[111] > $tab->[102]) or ($tab->[111] > $tab->[120]);
	print "{\\PP}" if ($tab->[102] ne 'inf' && $tab->[111] * ($tab->[114] + 1) < $tab->[102]);
	print "\\textbf" if ($tab->[111] < $tab->[120]);
	print "{$tab->[111]} & "; # states
	print "$tab->[113] & "; # transitions
	print "\\textbf" if ($tab->[114] > $tab->[6]);
	print "{$tab->[114]} & "; # acc
	printf("%.2f ", $tab->[118]);
    }

    print "\\\\\n";
}
print "\\end{tabular}\n";
print "\\end{document}\n";
