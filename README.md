SWATd
=====

SWATd lets you configure 'sensors' that check your PC's external environment.
When enough sensors 'fail', SWATd will run a script for you.

Sensors are commands or scripts that get executed repeatedly. A sensor is said
to fail when its exit code makes a transition from zero (working) to non-zero
(not working). This makes configuration easy and powerful. For example, you can
make a sensor that checks if your website is online, and then make a command to
alert you when the sensor fails.

SWATd was originally written as a tool to defend against theft by criminals or
to detect when your computer is captured by police. For example, you can set
a sensor to detect if your WiFi network is in range, and when it goes out of
range, automatically unmount encrypted volumes. So if someone steals your laptop
from your house, your data will be safe. Since SWATd only counts the failure
when the sensor *changes* from a "WiFi in range" state to a "WiFi out of range"
state, if you use your laptop somewhere else, you don't need to worry about
disabling SWATd every time you leave your house.

**WARNING:** While this may be helpful for some, there are significant risks.
For one, in some countries, including the United States, you could go to jail on
obstruction of justice charges just for *running* SWATd, even though you are
innocent. Second, SWATd is not perfect: law enforcement or a smart thief can
still dump your RAM, thus getting your encryption keys, before doing anything
that would make a sensor fail. Use with caution, and consult an attorney first.

Building and Installing
-----------------------

To build SWATd, `cd` into the source code directory and run `make`. This will
create a `swatd` executable. If you want to install it as a daemon, refer to
your operating system's manuals. To run SWATd from a terminal (non-daemon), pass
the `-s` option.

### Arch Linux

To install SWATd on Arch Linux, copy `swatd` into `/usr/bin`:

    # make
    # install swatd /usr/bin/

Create the configuration file (See the Configuration section below):

    # mkdir /etc/swatd
    # chmod 700 /etc/swatd
    # vim /etc/swatd/swatd.conf

If you want SWATd to start when you boot, add the following to
`/etc/systemd/system/swatd.service`.

    [Unit]
    Description=SWATd
    
    [Service]
    Type=forking
    PIDFile=/var/run/swatd.pid
    ExecStart=/usr/bin/swatd -p /var/run/swatd.pid
    Restart=on-abort
    
    [Install]
    WantedBy=multi-user.target

Then run:

    # systemctl enable swatd.service
    # systemctl start swatd.service

You can check the status of SWATd by running:

    # systemctl status swatd.service

Read SWATd's log entries by running:

    # journalctl /usr/bin/swatd

### Debian 

To install SWATd on Debian, copy `swatd` into `/usr/bin`:

    # make
    # install swatd /usr/bin/

Create the configuration file (See the Configuration section below):

    # mkdir /etc/swatd
    # chmod 700 /etc/swatd
    # vim /etc/swatd/swatd.conf

Then copy `swatd.init` to `/etc/init.d/` and enable it:

    # cp swatd.init /etc/init.d/swatd
    # update-rc.d swatd defaults

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

`interval` is the number of seconds to wait between sensor checks. `threshold`
is the number of sensors that must fail before assuming you are being raided.
`execute` is the command to execute when you are being raided.

Here is an example configuration file:

    # This configuration makes SWATd continually check if /tmp/foobar exists. If
    # /tmp/foobar stops existing (goes from existing to not existing), SWATd will
    # write some text to the file /tmp/ran.
    
    # =============================================================================
    # The number of seconds to wait between sensor checks.
    # =============================================================================
    interval: 30
    
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
    # A sensor has 'failed' when the exit code transisions from zero to non-zero.
    # If a sensor's exit code is transitions from zero to 255, the command will be 
    # executed immediately regardless of the 'threshold' setting, and the failure
    # count will not be incremented.
    # WARNING: Sensor commands MUST terminate.
    # =============================================================================
    
    test -e /tmp/foobar
