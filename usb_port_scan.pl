#!/usr/bin/perl
# USB Port Scanner — включвай USB устройство в различни портове
# Следи dmesg за нови USB events, казва bus/port/controller (EHCI/xHCI)
#
# Употреба: perl usb_port_scan.pl
# Включвай USB флашка порт по порт, натискай Enter

use strict;
use warnings;

$| = 1;

print "=== USB Port Scanner ===\n";
print "Включвай USB устройство в различни портове.\n";
print "Натисни Enter след като включиш в нов порт.\n\n";

my $n = 1;
while (1) {
    printf "Порт #%d — включи и натисни Enter: ", $n;
    my $input = <STDIN>;
    last unless defined $input;

    # Последен USB event от dmesg
    my @lines = grep { /new.*USB device|New USB device found/ } `dmesg -T 2>/dev/null`;
    unless (@lines) {
        print "  Не виждам USB event в dmesg\n\n";
        next;
    }

    my $last = $lines[-1];
    chomp $last;

    my ($busdev) = $last =~ /usb (\d+-[\d.]+)/;
    unless ($busdev) {
        print "  Не мога да парсна: $last\n\n";
        next;
    }

    my ($bus) = $busdev =~ /^(\d+)/;

    # Controller type
    my $driver = `cat /sys/bus/usb/devices/usb${bus}/driver_name 2>/dev/null` || '';
    chomp $driver;

    my $pci = '';
    my $link = readlink("/sys/bus/usb/devices/usb${bus}") || '';
    ($pci) = $link =~ /(\d+:\d+:\d+\.\d+)/g if $link;

    my $type = ($driver =~ /ehci/i) ? '*** EHCI (USB 2.0) ***' : 'xHCI (USB 3.0)';

    # Product info
    my $product = `cat /sys/bus/usb/devices/${busdev}/product 2>/dev/null` || '';
    chomp $product;

    printf "  #%d: Bus=%s  Dev=%s  PCI=%s  %s  [%s]\n\n",
           $n, $bus, $busdev, $pci, $type, $product;

    $n++;
}
