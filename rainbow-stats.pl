#!/usr/bin/perl
# The above line is modified by ./Makefile to match the system's
# installed location for Perl.

# Script to process the output from Andrew's rainbow program and produce
# useful summaries of the results. Feed the results intot stdin and 
# all the summaries will arrive on stdout

# If you pass the `-s' command line argument, print only the accuracy
# average and standard deviation.

# setup some default values
$total_accuracy = 0.0;

# When this is zero, only print accuracy average and std.dev.
$verbosity = 1;

if ($#ARGV >= 0 && $ARGV[0] == "-s") {
    $verbosity = 0;
    shift;
}

# Read in the first #
$line = <>;

$trial = 0;
while (&read_trial() != 0) {

    # OK - Lets start with accuracy
    &calculate_accuracy();

    # Now, how about a confusion matrix.
    &confusion();

    $trial++;

}

# Maybe some summary?
# We've had $trial trials

&overall_accuracy();

exit;

# Function to read in the results for one trial into three arrays - @ids, 
# @actual_classifications and @predicted_classifications
# What is the English description of these?
# @ids - 
# @actual_classifications - 
# @predicted_classifications - 
sub read_trial {
    
    undef @ids;
    undef @actual_classifications;
    undef @predicted_classifications;
    undef %classes_to_codes;
    undef @codes_to_classes;

    while (($line = <>) && ($line !~ /^\#[0-9]+$/)) {
	
	chop $line;

	@line = split(' ', $line);
	
	push(@ids, shift @line);

	# Ensure we have a code for the actual class
	if (grep(/^$line[0]$/, @codes_to_classes) == 0) {
	    $classes_to_codes{$line[0]} = @codes_to_classes;
	    push(@codes_to_classes, $line[0]);
	}

	push(@actual_classifications, shift @line);
			    
	push(@predicted_classifications, [ @line ]);

	# Make sure we have codes for everything
	foreach $pred (@line) {
	    $pred =~ /^(.+):[\.0-9e+\-]+$/;

	    if (grep(/^$1$/, @codes_to_classes) == 0) {
		$classes_to_codes{$1} = @codes_to_classes;
		push(@codes_to_classes, $1);
	    }
	}
    }

    if (@ids > 0) {
	return 1;
    } else {
	return 0;
    }
}

# Function to take the three arrays and calculate the accuracy of the
# run
sub calculate_accuracy {
    if ($verbosity > 0) {
	print "Trial $trial\n\n";
    }
    # Initialize the variables in which we'll gather stats
    $correct = 0;
    $total = 0;

    for ($i = 0; $i < @ids; $i++) {
	$predicted_classifications[$i][0] =~ /^(.+):[\.0-9e+\-]+$/;
	if ($actual_classifications[$i] eq $1) {
	    $correct++;
	}
	$total++;
    }

    $accuracy = ($correct * 100) / $total;
    $trial_accuracy[$trial] = $accuracy;
    $total_accuracy += $accuracy;
    if ($verbosity > 0) {
	printf ("Correct: %d out of %d (%.2f)\n", $correct, $total, $accuracy);
    }
}

sub overall_accuracy {
    # Calculte the overall (overall) accuracy
    $overall_accuracy = $total_accuracy / $trial;

    # Calculate the standard deviation of Overall Accuracy
    $overall_accuracy_stddev = 0;
    for ($i = 0; $i < $trial; $i++) {
	$diff_from_mean = $overall_accuracy - $trial_accuracy[$i];
	$overall_accuracy_stddev += $diff_from_mean * $diff_from_mean;
    }
    $overall_accuracy_stddev = sqrt ($overall_accuracy_stddev / $trial);

    if ($verbosity > 0) {
	printf ("Overall_accuracy_and_stderr %.2f %.2f\n", 
		$overall_accuracy, 
		$overall_accuracy_stddev / sqrt($trial));
    } else {
	printf ("%.2f %.2f\n", 
		$overall_accuracy, 
		$overall_accuracy_stddev / sqrt($trial));
    }
}

# Function to produce a confusion matrix from the data
sub confusion {
    
    undef @confusion;

    if (! $verbosity > 0) {
	return;
    }

    print "\n - Confusion details\n";
    # Loop over all the examples
    for ($i = 0; $i < @ids; $i++) {

	$actual = $actual_classifications[$i];
	$actual_code = $classes_to_codes{$actual};

	$predicted_classifications[$i][0] =~ /^(.+):[\.0-9e+\-]+$/;
	$predicted_code = $classes_to_codes{$1};

	$confusion[$actual_code][$predicted_code] += 1;
    }

    # Now print out the matrix
    for ($i = 0; $i < @codes_to_classes; $i++) {
	print "Actual: $codes_to_classes[$i]\n";

	for ($j = 0; $j < @codes_to_classes; $j++) {
	    print "$codes_to_classes[$j]:$confusion[$i][$j] ";
	}
	print "\n";
    }
    print "\n";
}
