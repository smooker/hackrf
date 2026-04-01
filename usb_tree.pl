#!/usr/bin/perl
# USB Tree — рекурсивно дърво от /sys/bus/usb/devices
use strict;
use warnings;

my $base = "/sys/bus/usb/devices";

sub read_attr {
    my ($path, $attr) = @_;
    my $file = "$path/$attr";
    return "" unless -f $file;
    open my $fh, '<', $file or return "";
    chomp(my $val = <$fh>);
    close $fh;
    return $val;
}

sub get_driver {
    my ($path) = @_;
    my $link = readlink("$path/driver") || "";
    my ($drv) = $link =~ m{/([^/]+)$};
    return $drv || "";
}

sub print_device {
    my ($devpath, $indent) = @_;
    my $path = "$base/$devpath";
    return unless -d $path;

    my $product  = read_attr($path, "product");
    my $mfr      = read_attr($path, "manufacturer");
    my $vid      = read_attr($path, "idVendor");
    my $pid      = read_attr($path, "idProduct");
    my $speed    = read_attr($path, "speed");
    my $serial   = read_attr($path, "serial");
    my $bcddev   = read_attr($path, "bcdDevice");
    my $maxpow   = read_attr($path, "bMaxPower");
    my $nifs     = read_attr($path, "bNumInterfaces");
    my $class    = read_attr($path, "bDeviceClass");

    # Get driver from first interface
    my $driver = "";
    my @ifs = glob("$path/$devpath:*");
    if (@ifs) {
        $driver = get_driver($ifs[0]);
    }

    my $label = "";
    if ($class eq "09") {
        my $ports = read_attr($path, "maxchild");
        $label = "Hub ($ports ports)";
    } elsif ($product) {
        $label = $product;
        $label .= " [$mfr]" if $mfr && $mfr ne $product;
    } else {
        $label = "unknown";
    }

    my @info;
    push @info, "$vid:$pid" if $vid;
    push @info, "${speed}M" if $speed;
    push @info, "drv=$driver" if $driver && $driver ne "hub";
    push @info, "serial=$serial" if $serial && $serial ne "0000000000000000";
    push @info, "ver=$bcddev" if $bcddev && $bcddev ne "0000";
    push @info, "power=$maxpow" if $maxpow && $maxpow ne "0mA";
    push @info, "ifs=$nifs" if $nifs && $nifs > 1;

    $label .= "  (" . join(", ", @info) . ")" if @info;

    print "${indent}├── $devpath: $label\n";

    # Show empty ports for hubs
    if ($class eq "09") {
        my $maxchild = read_attr($path, "maxchild") || 0;
        my %used;
        for my $child (glob("$path/$devpath.*")) {
            my $name = $child;
            $name =~ s{.*/}{};
            next if $name =~ /:/;
            my ($port) = $name =~ /\.(\d+)$/;
            $used{$port} = $name if $port;
        }
        for my $p (1..$maxchild) {
            if ($used{$p}) {
                print_device($used{$p}, "$indent│   ");
            } else {
                print "${indent}│   ├── port $p: (empty)\n";
            }
        }
    } else {
        # Non-hub children (rare)
        for my $child (sort glob("$path/$devpath.*")) {
            my $name = $child;
            $name =~ s{.*/}{};
            next if $name =~ /:/;
            print_device($name, "$indent│   ");
        }
    }
}

# Main — iterate root hubs
my @roots = sort { ($a =~ /(\d+)/)[0] <=> ($b =~ /(\d+)/)[0] }
            grep { /^usb\d+$/ } map { s{.*/}{}r } glob("$base/usb*");

for my $root (@roots) {
    my $path = "$base/$root";
    my $bus = $root;
    $bus =~ s/usb//;

    my $speed = read_attr($path, "speed");

    # Get PCI driver name
    my $realpath = readlink($path) || "";
    my @pci_ids = $realpath =~ /(\d+:\d+:\d+\.\d+)/g;
    my $pci = $pci_ids[-1] || "";
    my $pci_driver = "";
    if ($pci) {
        $pci_driver = get_driver("/sys/bus/pci/devices/$pci");
    }

    my $type;
    if ($pci_driver =~ /ehci/i) {
        $type = "*** EHCI ***";
    } elsif ($pci_driver =~ /xhci/i) {
        $type = "xHCI";
    } elsif ($pci_driver) {
        $type = $pci_driver;
    } else {
        $type = "unknown";
    }

    print "Bus $bus ($type  PCI=$pci  ${speed}M)\n";

    # Top-level devices
    my @children = sort glob("$base/$bus-*");
    my %seen;
    for my $child (@children) {
        my $name = $child;
        $name =~ s{.*/}{};
        next if $name =~ /:/;
        next if $name =~ /^\d+-\d+\./;
        print_device($name, "  ");
    }
    print "\n";
}
