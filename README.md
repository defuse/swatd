SWATd
=====

SWATd is a daemon for running scripts when your house gets raided by the police.
For example, if you use any kind of encryption, you can use SWATd to destroy the
keys, instead of hoping the police are stupid enough to unplug your computer.

SWATd lets you configure 'sensors' that check your PC's external environment.
When enough sensors 'fail', SWATd will conclude that you are being raided and
will run the script you have configured.

Sensors are just commands (or scripts). A sensor 'fails' when its return code
makes a transition from zero to non-zero. This makes configuration easy and
powerful. For example, you could create a sensor that fails when your WiFi
network is out of range, and another that fails when your ethernet cable is
unplugged.

Building and Installing
-----------------------

To build SWATd, `cd` into the source code directory and run `make`. This will
create a `swatd` executable. If you want to install it as a daemon, refer to
your operating system's manuals. To run SWATd from a terminal (non-daemon), pass
the `-s` option.

Configuration
-------------

By default, SWATd looks for a configuration file in `/etc/swatd/swatd.conf`.
Alternatively, you can provide a configuration file path to SWATd with the `-c`
option. In any case, the configuration file must not be world writable, or SWATd
will refuse to run.

The configuration file syntax is extremely simple. There are only three options:
`interval`, `threshold`, and `execute`. To set a value for one of the options,
begin a line with its name, followed by a colon, followed by the value.
Everything after a '#' is treated as a comment (ignored). Blank lines are
ignored. All other lines define a sensor command.

Here is an example configuration file:

    # This configuration makes SWATd continually check if /tmp/foobar exists. If
    # /tmp/foobar stops existing (goes from existing to not existing), SWATd will
    # write some text to the file /tmp/ran.
    
    # =============================================================================
    # The number of seconds to wait between sensor checks.
    # =============================================================================
    interval: 10
    
    # =============================================================================
    # The number of sensors that must 'fail' at the same time.
    # =============================================================================
    threshold: 1
    
    # =============================================================================
    # The command to execute when 'threshold' sensors fail.
    # =============================================================================
    execute: echo "haiii" > /tmp/ran
    
    # =============================================================================
    # Sensor commands.
    # A sensor has 'failed' when the return code transisions from zero to non-zero.
    # WARNING: Sensor commands MUST terminate.
    # =============================================================================
    
    test -e /tmp/foobar
