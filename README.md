dvdripper
=========

Remove CSS from a DVD .iso file.

Project motivated by the need to recover scratched DVDs within my
collection.  Most DVD ripping software doesn't handle scratches well,
as they either give you the option to die, ignore or retry the error.

ddrescue (from GNU) actually can work very well for this as it keeps a
bitmap of what content one was able to read and then can fill in the
missing pieces later, however, it doesn't decrypt data.  Most
decryption software doesn't decrypt ISO's with CSS scrambled sectors
(though VLC and other tools built around libdvdcss can play them fine
due to libdvdcss brute forcing the crypto), as they expect ISO's to be
already decrypted and query the device for the content's keys instead
of brute forcing the crypto.

This software decrypts the scrambled sectors in place enabling you to
then extract the iso to a file system or use another ripping tool to
extract the desired content from the already decrypted image.

For protection mechanisms that protect the disc by including invalid
sectors, ddrescue will mark those sectors as unreadable, but it
doesn't matter as they aren't needed.  However. if the disc is
protected with that, and it's damaged (such as scratches that make
needed data unreadable), one will not be able to tell the difference.

If the disc has protection mechanisms that cause it to be viewed as a
very large file system (i.e. a DVD that looks like a 100GB file
system), this will remove the encryption and enable you to mount the
image, but you will not be able to simply extract the contents of the
images (without wasting a lot of space).  Tools that understand the
IFO structure are neccessary to extract the correct files.


Usage
-----
1. Use `ddrescue` to get a `.iso` file from your DVD
2. Use `dvdripper` to remove CSS from the `.iso` file:
```
$ dvdripper mydvd.iso
```

The `.iso` file is directly modified.
