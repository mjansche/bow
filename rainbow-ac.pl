#!/usr/bin/perl
# The above line is modified by ./Makefile to match the system's
# installed location for Perl.

$usage = <<USAGE;
USAGE: ac [-bins] interval classes|'all' < andrew-files
USAGE

if ($ARGV[0] eq '-bins')
{
    $bins = 1;
    shift @ARGV;
}
($interval, @classes) = @ARGV;

&usage_msg unless $interval && @classes;


if (@classes == 1 && $classes[0] eq 'all')
{
    $all_classes = 1;
}
else
{
    for (@classes) { $classes{$_} = 1 }
}


print STDERR "Reading...\n"; 

while ($line = <STDIN>)
{
    if (($actual_class, $predicted) = $line =~ /^\S+\s+(\S+)\s+(\S+)/)
    {
	if (($predicted_class, $confidence) = $predicted =~ /([^:]+):(.*)/)
	{
	    push(@predictions, 
		 [ $confidence, $predicted_class, $actual_class ]);
	}
    }
}

print STDERR "Got ", $predictions, " records.\nSorting...\n";

@predictions = sort { $b->[0] <=> $a->[0] } @predictions;

$at_interval = $interval;

print STDERR "Coverage  Accuracy\n";

for $prediction (@predictions)
{
    # coverage
    if ($all_classes || $classes{$prediction->[2]})
    {
	++$seen_count;	       
    }
}


for $prediction (@predictions)
{
    if ($all_classes || $classes{$prediction->[1]})
    {
	++$predicted_count;
	++$correct_count, ++$all_correct_count
	    if $prediction->[1] eq $prediction->[2];
    }

    $cov_percent = 100 * $all_correct_count / $seen_count;

    if ($cov_percent >= $at_interval)
    {
	$acc_percent = 100 * $correct_count / $predicted_count;
	$at_interval += $interval;
	printf("%6.2f  %8.2f (%3d correct of %3d predicted, %7f confidence)\n", $cov_percent, $acc_percent, $correct_count, $predicted_count, $prediction->[0]);
#	print $cov_percent, ' ', $acc_percent, "\n";
	$correct_count = $predicted_count = 0 if $bins;
    }

#    print $seen_count, ' ', $predicted_count, ' ', $correct_count, "\n";
    $final_confidence = $prediction->[0];
}

$acc_percent = 100 * $correct_count / $predicted_count;
printf("%6.2f  %8.2f (%3d correct of %3d predicted, %7f confidence)\n", $cov_percent, $acc_percent, $correct_count, $predicted_count, $final_confidence);

sub usage_msg
{
    print STDERR $usage;
    exit 1;
}
