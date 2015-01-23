SWATd
=====

WARNING
---------

**READ THIS:** While this software may have a place, I (the author) strongly
**do not recommend it**. Frankly, when I saw this repository had over 100
"stars" on GitHub, I was shocked. I never intended this to be used so widely.
**If you need to rely on SWATd, you have already lost.** First, law enforcement
can just dump your RAM on the spot through your Firewire port without touching
anything. Second, you're technically destroying evidence, and you could go to
jail just for that. If you are actually using SWATd as a defense against law
enforcement (which I **strongly do not recommend**), at least consult a lawyer.
I feel that by releasing this code under the name "SWATd" and relating it to any
sort of defense against law-enforcement, I am no better than those who peddle
snakeoil crypto. I sincrely apologize for this, and I can only hope no one has
been harmed. The code has been removed, but if you *really* need it, I've
created a branch pointing to a previous commit.

Old Description
----------------

SWATd is a daemon for running scripts when your house gets raided by the police
(or broken into by criminals).  For example, if you use any kind of encryption,
you can use SWATd to destroy the keys, instead of hoping the police (or
criminals) are stupid enough to unplug your computer.  SWATd can also be used
for more mundane things like sending an email notification when a server goes
down.

SWATd lets you configure 'sensors' that check your PC's external environment.
When enough sensors 'fail', SWATd will conclude that you are being raided and
will run the script you have configured.

Sensors are commands (or scripts) that are repeatedly executed. A sensor 'fails'
when its exit code makes a transition from zero to non-zero. This makes
configuration easy and powerful. For example, you could create a sensor that
fails when your WiFi network is out of range, and another that fails when your
Ethernet cable is unplugged.

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
