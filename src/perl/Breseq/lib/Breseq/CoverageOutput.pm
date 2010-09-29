###
# Pod Documentation
###

=head1 NAME
Breseq::AlignmentOutput.pm

=head1 SYNOPSIS

Module for making alignments for display from SAM databases.

=head1 DESCRIPTION

=head1 AUTHOR

Jeffrey Barrick
<jeffrey@barricklab.org>

=head1 COPYRIGHT

Copyright 2009.  All rights reserved.

=cut

###
# End Pod Documentation
###

use strict;

package Breseq::CoverageOutput;
use vars qw(@ISA);
use Bio::Root::Root;
@ISA = qw( Bio::Root::RootI );

use CGI qw/:standard *table *Tr *code *td start_b end_b start_i end_i/;

use Bio::DB::Sam;
use Breseq::Fastq;
use Breseq::Shared;
use Data::Dumper;


## Workaround to avoid too many open files... bug in Bio::DB::Sam
my %open_bam_files;

sub new
{
	my($caller,@args) = @_;
	my $class = ref($caller) || $caller;
	my $self = new Bio::Root::Root($caller, @args);

	bless ($self, $class);
	($self->{fasta}, $self->{bam}, $self->{path}) = $self->Bio::Root::RootI::_rearrange([qw(FASTA BAM PATH)], @args);
	$self->{path} = '.' if (!defined $self->{path});
	$self->{r_script} = $FindBin::Bin . "/../lib/perl5/Breseq/plot_coverage.r";
	
	## Workaround to avoid too many open files... bug in Bio::DB::Sam
	my $bam_path = $self->{bam};
	my $fasta_path = $self->{fasta};
	if (!defined $open_bam_files{$bam_path.$fasta_path})
	{
		$open_bam_files{$bam_path.$fasta_path} = Bio::DB::Sam->new(-bam =>$bam_path, -fasta=>$fasta_path);
	}
	$self->{bam} = $open_bam_files{$bam_path.$fasta_path};
	
	return $self; 
}


sub plot_coverage
{
	my ($self, $region, $output, $options) = @_;

	## Set up defaults
	$options = {} if (!defined $options);
	$options->{pdf} = 0 if (!defined $options->{pdf});
	$options->{resolution} = 600 if (!defined $options->{resolution});
	$options->{total_only} = 0 if (!defined $options->{total_only});
	$options->{shaded_flanking} = 0 if (!defined $options->{shaded_flanking}); #how much gray on each end
	
	my ($seq_id, $start, $end, $insert_start, $insert_end);
	($seq_id, $start, $end, $insert_start, $insert_end, $region)  = Breseq::Shared::region_to_coords($region, $self->{bam});
	$self->throw("Invalid region $region") if (!$seq_id || !$start || !$end);
		
	## extend the region and re-check
	my $extended_start = $start - $options->{shaded_flanking};
	$extended_start = 0 if ($extended_start < 0);
	my $extended_end = $end + $options->{shaded_flanking};
	#call to region_to_coords will fix the end if it has been extended too far
	
	my $extended_region = "$seq_id:$extended_start-$extended_end";
	my ($extended_seq_id, $extended_insert_start, $extended_insert_end);
	($extended_seq_id, $extended_start, $extended_end, $extended_insert_start, $extended_insert_end, $extended_region)  = Breseq::Shared::region_to_coords($extended_region, $self->{bam});
			
	my $size = $extended_end - $extended_start + 1;
	
	$output = $region if (!defined $output);
	$output =~ s/:/_/g;
	$output .= ($options->{pdf}) ? ".pdf" : ".png";
	
	## if no guidance provided about how much to downsample, aim for 1E2-1E3 total points.
	my $downsample = $options->{downsample};
	my $resolution = $options->{resolution};
	if (!defined $downsample)
	{
		$downsample = int($size/$resolution);
		$downsample = 1 if ($downsample < 1);
	}
	
	my $tmp_coverage = "$self->{path}/$$.coverage.tab";
	$self->tabulate_coverage($tmp_coverage, $extended_region, {downsample => $downsample} );
	
	my $log_file_name = "$self->{path}/$$.r.log";
	Breseq::Shared::system("R --vanilla in_file=$tmp_coverage out_file=$output pdf_output=$options->{pdf} total_only=$options->{total_only} window_start=$start window_end=$end < $self->{r_script} > $log_file_name");
	
	#clean up
	unlink $tmp_coverage;
	unlink $log_file_name;
}

## Rewrite this in C++ for speed!
sub tabulate_coverage
{
	my ($self, $tmp_coverage, $region, $options) = @_;
	my $downsample = $options->{downsample};
	$downsample = 1 if (!defined $downsample);
	my $bam = $self->{bam};
		
	my ($seq_id, $start, $end, $insert_start, $insert_end);
	($seq_id, $start, $end, $insert_start, $insert_end, $region) = Breseq::Shared::region_to_coords($region, $bam);
	
	## Open file for output
	open COV, ">$tmp_coverage";
	COV->autoflush(1);
	print COV join("\t", 'position', 'unique_top_cov', 'unique_bot_cov', 'redundant_top_cov', 'redundant_bot_cov') . "\n";
	our $coverage;

	###
	##  Behold, the dreaded SLOW fetch function...
	###
	
	my $fetch_function = sub {
		my ($a) = @_;
  						
		my $redundancy = $a->aux_get('X1');
		my $reversed = $a->reversed;
		my $strand = $reversed ? -1 : +1;
		
		##### Update coverage if this is not a deletion in read relative to reference
		### Count trimmed reads here, but not when looking for short indel mutations...	
		if ($redundancy == 1)
		{	
			$coverage->{unique}->{$strand}++;
		}
		else
		{
			$coverage->{redundant}->{$strand} += 1/$redundancy;			
#			$this_position_coverage->{raw_redundant}->{$strand}++;			
		}		
	}; 

	$start = $downsample * int($start/$downsample);
	$start = 1 if ($start < 1);
	
	$end = $downsample * int((($end-1)/$downsample) + 1);
	my $reference_length = $bam->length($seq_id);
	$end = $reference_length if ($end > $reference_length);	
				
	for (my $pos = $start; $pos <= $end; $pos += $downsample)
	{		
		#initialize coverage observations
		$coverage->{unique} = {'-1' => 0, '1' => 0, 'total' => 0};
		$coverage->{redundant} = {'-1' => 0, '1' => 0, 'total' => 0};
#		$coverage->{raw_redundant} = {'-1' => 0, '1' => 0, 'total' => 0};
				
		my $fetch_region = $seq_id . ":" . $pos . "-" . $pos;
#		print "$fetch_region\n";
		$bam->fetch($fetch_region, $fetch_function);
		
		#sum up coverage observations
		$coverage->{unique}->{total} = $coverage->{unique}->{-1} + $coverage->{unique}->{+1};
		$coverage->{redundant}->{total} = $coverage->{redundant}->{-1} + $coverage->{redundant}->{+1};
#		$coverage->{raw_redundant}->{total} = $coverage->{raw_redundant}->{-1} + $coverage->{raw_redundant}->{+1};

		my $tu = $coverage->{unique};
		my $tr = $coverage->{redundant};
#		my $trr = $this_position_coverage->{raw_redundant};
		print COV join("\t", $pos, $tu->{-1}, $tu->{1}, $tr->{-1}, $tr->{1}) . "\n";
	}
	
	close COV;
}

return 1;

