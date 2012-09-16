dvdripper
=========

extract CSS from a DVD.

Project motivated by the need to recover scratched DVDs within my collection.  Most DVD ripping software doesn't handle scratches well, as they either give you the option to die, ignore or retry the error.

ddrescue (from GNU) actually can work very well for this as it keeps a bitmap of what content one was able to read and then can fill in the missing pieces later, however, it doesn't decrypt data.  Most decryption software doesn't decrypt ISO's with CSS scrambled sectors (though VLC and other tools built around libdvdcss can play them fine due to libdvdcss brute forcing the crypto), as they expect ISO's to be already decrypted and query the device for the content's keys instead of brute forcing the crypto.

This software decrypts the scrambled sectors in place enabling you to then extract the iso to a file system or use another ripping tool to extract the desired content from the already decrypted image.