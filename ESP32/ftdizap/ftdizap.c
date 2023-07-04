// Tweak FTDI EEPROM

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include <libusb-1.0/libusb.h>
#include <libftdi1/ftdi.h>
#include <assert.h>

int debug = 0;
#define	ELEN	0x800           /* len of EEPROM */
#define	SPOS	0x0A0           /* Start of strings */
#define	CSUM	0x0FE           /* Pos of checksum */
unsigned char buf[ELEN];
unsigned char was[ELEN];
unsigned int spos = SPOS;

// Note CBUS mode documentation is lacking, depends on chip... See https://www.intra2net.com/en/developer/libftdi/documentation/ftdi_8c_source.html
// FT230/FT231 :-
// 0    TRISTATE
// 1    TXLED
// 2    RXLED
// 3    TXRXLED
// 4    PWREN
// 5    SLEEP
// 6    DRIVE_0
// 7    DRIVE_1
// 8    IOMODE
// 9    TXDEN
// 10   CLK24
// 11   CLK12
// 12   CLK6
// 13   BAT_DETECT
// 14   BAT_DETECT#
// 15   I2C_TXE#
// 16   I2C_RXF#
// 17   VBUS_SENSE
// 18   BB_WR#
// 19   BBRD#
// 20   TIME_STAMP
// 21   AWAKE#

#define inline

inline unsigned char
getbit (unsigned int n, unsigned int b)
{
   assert (n < ELEN);
   assert (b < 8);
   return (buf[n] >> b) & 1;
}

void
setbit (unsigned int n, unsigned int b, int v, const char *desc)
{
   assert (n < ELEN);
   assert (b < 8);
   assert (v < 2);
   int was = getbit (n, b);
   if (debug)
      fprintf (stderr, "0x%d    %s", was, desc);
   if (v < 0)
   {
      if (debug)
         fprintf (stderr, "\n");
      return;
   }
   if (v == was)
   {
      if (debug)
         fprintf (stderr, " unchanged\n");
      return;
   }
   if (debug)
      fprintf (stderr, " changed to 0x%d\n", v);
   buf[n] = (buf[n] & ~(1 << b)) | (v << b);
}

inline unsigned char
getbit2 (unsigned int n, unsigned int b)
{
   assert (n < ELEN);
   assert (b < 7);
   return (buf[n] >> b) & 3;
}

void
setbit2 (unsigned int n, unsigned int b, int v, const char *desc)
{
   assert (n < ELEN);
   assert (b < 8);
   assert (v < 4);
   int was = getbit (n, b);
   if (debug)
      fprintf (stderr, "0x%d    %s", was, desc);
   if (v < 0)
   {
      if (debug)
         fprintf (stderr, "\n");
      return;
   }
   if (v == was)
   {
      if (debug)
         fprintf (stderr, " unchanged\n");
      return;
   }
   if (debug)
      fprintf (stderr, " changed to 0x%d\n", v);
   buf[n] = (buf[n] & ~(3 << b)) | (v << b);

}

inline unsigned char
getbyte (unsigned int n)
{
   assert (n < ELEN);
   return buf[n];
}

void
setbyte (unsigned int n, int v, const char *desc)
{
   assert (n < ELEN);
   assert (v < 0x100);
   unsigned char was = getbyte (n);
   if (debug)
      fprintf (stderr, "0x%02X   %s", was, desc ? : "");
   if (v < 0)
   {
      if (debug)
         fprintf (stderr, "\n");
      return;
   }
   if (v == was)
   {
      if (debug)
         fprintf (stderr, " unchanged\n");
      return;
   }
   if (debug)
      fprintf (stderr, " changed to 0x%02X\n", v);
   buf[n] = v;
}

inline unsigned short
getword (unsigned int n)
{
   assert (n < ELEN / 2);
   return (buf[n * 2 + 1] << 8) | (buf[n * 2]);
}

void
setword (unsigned int n, int v, const char *desc)
{
   assert (n < ELEN / 2);
   assert (v < 0x10000);
   unsigned short was = getword (n);
   if (debug)
      fprintf (stderr, "0x%04X %s", was, desc ? : "");
   if (v < 0)
   {
      if (debug)
         fprintf (stderr, "\n");
      return;
   }
   if (v == was)
   {
      if (debug)
         fprintf (stderr, " unchanged\n");
      return;
   }
   if (debug)
      fprintf (stderr, " changed to 0x%04X\n", v);
   buf[n * 2 + 1] = (v >> 8);
   buf[n * 2] = v;
}

char *
getstring (unsigned int n, const char *desc)
{
   assert (n < ELEN / 2);
   int pos = getbyte (n * 2);
   int len = getbyte (n * 2 + 1);
   if (pos < SPOS || pos + len >= CSUM || (len && (len < 2 || buf[pos] != len || buf[pos + 1] != 3)))
   {
      warnx ("Bad string %s at 0x%04X+%d", desc, pos, len);
      return "";
   }
   len = (len - 2) / 2;
   char *ret = malloc (len + 1);
   if (!ret)
      errx (1, "malloc");
   for (int a = 0; a < len; a++)
      ret[a] = buf[pos + 2 + a * 2];
   ret[len] = 0;
   /* TODO does that mean unicode, we could UTF stuff? */
   return ret;
}

void
setstring (unsigned int n, const char *s, const char *desc)
{
   char *was = getstring (n, desc);
   if (!s)
      s = was;
   if (debug)
   {
      if (strcmp (s, was))
         fprintf (stderr, "%s changed from \"%s\" to \"%s\"\n", desc, was, s);
      else
         fprintf (stderr, "%s unchanged \"%s\"\n", desc, was);
   }
   int len = 2 + strlen (s) * 2;
   if (spos + len >= CSUM)
      errx (1, "Strings too long");
   buf[n * 2] = spos;
   buf[n * 2 + 1] = len;
   buf[spos++] = len;
   buf[spos++] = 0x03;          /* string */
   while (*s)
   {
      buf[spos++] = *s++;
      buf[spos++] = 0;
   }
   /* TODO does that mean unicode, we could UTF stuff? */
}

void
checksum (void)
{
   //Calculate and store checksum
   unsigned short c = 0xAAAA;
   for (int a = 0; a < 0x12; a++)
   {
      c ^= getword (a);
      c = (c << 1) | (c >> 15);
   }
   for (int a = 0x40; a < 0x7F; a++)
   {
      c ^= getword (a);
      c = (c << 1) | (c >> 15);
   }
   setword (0x7F, c, "Checksum");
}

int
main (int argc, const char *argv[])
{
   int cbusall = -1,
      cbus0 = -1,
      cbus1 = -1,
      cbus2 = -1,
      cbus3 = -1;
   int cbus0mode = -1,
      cbus1mode = -1,
      cbus2mode = -1,
      cbus3mode = -1,
      cbus4mode = -1,
      cbus5mode = -1,
      cbus6mode = -1;
   int vid = -1,
      pid = -1,
      release = -1;
   int matchvid = 0x0403,
      matchpid = 0x6015,
      matchindex = 0;
   int bcdenable = -1,
      forcepowerenable = -1,
      deactivatesleep = -1,
      rs485echosuppress = -1,
      extosc = -1,
      extoscfeedback = -1,
      cbuspinsetforvbussense = -1,
      loadd2xxorvcpdrive = -1;
   int enableusbremote = -1,
      selfpowered = -1;
   int maxpower = -1;
   int suspendpulldown = -1,
      enableserialnumber = -1,
      ft1249cpol = -1,
      ft1249bord = -1,
      ft1249flow = -1,
      disableschmitt = -1,
      inverttxd = -1,
      invertrxd = -1,
      invertrts = -1,
      invertcts = -1,
      invertdtr = -1,
      invertdsr = -1,
      invertdcd = -1,
      invertri = -1,
      dbusdrive = -1,
      dbusslow = -1,
      dbusschmitt = -1,
      cbusdrive = -1,
      cbusslow = -1,
      cbusschmitt = -1;
   int i2cslave = -1,
      i2cid1 = -1,
      i2cid2 = -1,
      i2cid3 = -1;
   int reset = 0;
   int dtr = -1;
   int rts = -1;
   const char *description = NULL;
   const char *manufacturer = NULL;
   const char *product = NULL;
   const char *serial = NULL;
   /* Defaults for the FT320X */
   poptContext optCon;
   {
      const struct poptOption optionsTable[] = {
         {"vendor", 'v', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &matchvid, 0, "Vendor ID to find device", "N"},
         {"product", 'p', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &matchpid, 0, "Product ID to find device", "N"},
         {"index", 'i', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &matchindex, 0, "Index to find device", "N"},
         {"cbus", 0, POPT_ARG_INT, &cbusall, 0, "All CBUS outputs", "0/1"},
         {"cbus0", 0, POPT_ARG_INT, &cbus0, 0, "CBUS0 output", "0/1"},
         {"cbus1", 0, POPT_ARG_INT, &cbus1, 0, "CBUS1 output", "0/1"},
         {"cbus2", 0, POPT_ARG_INT, &cbus2, 0, "CBUS2 output", "0/1"},
         {"cbus3", 0, POPT_ARG_INT, &cbus3, 0, "CBUS3 output", "0/1"},
         {"reset", 0, POPT_ARG_NONE, &reset, 0, "RTS reset"},
         {"dtr", 0, POPT_ARG_INT, &dtr, 0, "Set DTR", "0/1 (2=toggle)"},
         {"rts", 0, POPT_ARG_INT, &rts, 0, "Set RTS", "0/1 (2=toggle)"},
         {"vid", 'V', POPT_ARG_INT, &vid, 0, "Vendor ID", "N"},
         {"pid", 'P', POPT_ARG_INT, &pid, 0, "Product ID", "N"},
         {"manufacturer", 'M', POPT_ARG_STRING, &manufacturer, 0, "Manufacturer", "text"},
         {"serial", 'S', POPT_ARG_STRING, &serial, 0, "Serial Number", "text"},
         {"description", 'D', POPT_ARG_STRING, &description, 0, "Description", "text"},
         {"release", 'R', POPT_ARG_INT, &release, 0, "Release", "N"},
         {"cbus0-mode", '0', POPT_ARG_INT, &cbus0mode, 0, "CBUS0 mapping", "N"},
         {"cbus1-mode", '1', POPT_ARG_INT, &cbus1mode, 0, "CBUS1 mapping", "N"},
         {"cbus2-mode", '2', POPT_ARG_INT, &cbus2mode, 0, "CBUS2 mapping", "N"},
         {"cbus3-mode", '3', POPT_ARG_INT, &cbus3mode, 0, "CBUS3 mapping", "N"},
         {"cbus4-mode", '4', POPT_ARG_INT, &cbus4mode, 0, "CBUS4 mapping", "N"},
         {"cbus5-mode", '5', POPT_ARG_INT, &cbus5mode, 0, "CBUS5 mapping", "N"},
         {"cbus6-mode", '6', POPT_ARG_INT, &cbus6mode, 0, "CBUS6 mapping", "N"},
         {"bcd-enable", 0, POPT_ARG_INT, &bcdenable, 0, "BCD Enable", "0/1"},
         {"force-power", 0, POPT_ARG_INT, &forcepowerenable, 0, "Force Power Enable", "0/1"},
         {"deactivate-sleep", 0, POPT_ARG_INT, &deactivatesleep, 0, "De-activate Sleep", "0/1"},
         {"rs485-echo-support", 0, POPT_ARG_INT, &rs485echosuppress, 0, "RS485 Echo Suppression", "0/1"},
         {"ext-osc", 0, POPT_ARG_INT, &extosc, 0, "Ext. OSC", "0/1"},
         {"ext-osc-feedback", 0, POPT_ARG_INT, &extoscfeedback, 0, "Ext. OSC Feedback Resistor Enable", "0/1"},
         {"cbus-sense", 0, POPT_ARG_INT, &cbuspinsetforvbussense, 0, "CBUS pin set for VBUS sense", "0/1"},
         {"load-d2xx", 0, POPT_ARG_INT, &loadd2xxorvcpdrive, 0, "Load D2XX or VCP Driver", "0/1"},
         {"enable-usb-remote", 0, POPT_ARG_INT, &enableusbremote, 0, "Enable USB Remote wakeup", "0/1"},
         {"self-powered", 0, POPT_ARG_INT, &selfpowered, 0, "Self Powered", "0/1"},
         {"max-power", 0, POPT_ARG_INT, &maxpower, 0, "Max Power Value", "Nx2mA"},
         {"suspend-pull-down", 0, POPT_ARG_INT, &suspendpulldown, 0, "USB suspend pull down enable", "0/1"},
         {"enable-serial-number", 0, POPT_ARG_INT, &enableserialnumber, 0, "Enable/Disable USB Serial Number", "0/1"},
         {"ft1248-clock-high", 0, POPT_ARG_INT, &ft1249cpol, 0, "FT1248 Clock polarity high", "0/1"},
         {"ft1248-bit-order-lsb", 0, POPT_ARG_INT, &ft1249bord, 0, "FT1248 Bit Order LSB to MSB", "0/1"},
         {"ft1248-flow-control", 0, POPT_ARG_INT, &ft1249flow, 0, "FT1248 Flow Control Enable", "0/1"},
         {"disable-i2c-schmitt", 0, POPT_ARG_INT, &disableschmitt, 0, "Disable I2C Schmitt", "0/1"},
         {"invert-txd", 0, POPT_ARG_INT, &inverttxd, 0, "Invert TXD", "0/1"},
         {"invert-rxd", 0, POPT_ARG_INT, &invertrxd, 0, "Invert RXD", "0/1"},
         {"invert-rts", 0, POPT_ARG_INT, &invertrts, 0, "Invert RTS", "0/1"},
         {"invert-cts", 0, POPT_ARG_INT, &invertcts, 0, "Invert CTS", "0/1"},
         {"invert-dtr", 0, POPT_ARG_INT, &invertdtr, 0, "Invert DTR", "0/1"},
         {"invert-dsr", 0, POPT_ARG_INT, &inverttxd, 0, "Invert DSR", "0/1"},
         {"invert-dcd", 0, POPT_ARG_INT, &invertdcd, 0, "Invert DCD", "0/1"},
         {"invert-ri", 0, POPT_ARG_INT, &invertri, 0, "Invert RI", "0/1"},
         {"dbus-drive", 0, POPT_ARG_INT, &dbusdrive, 0, "DBUS Drive Current Strength", "0-3"},
         {"dbus-slow", 0, POPT_ARG_INT, &dbusslow, 0, "DBUS Slew Rate Slow", "0/1"},
         {"dbus-schmitt", 0, POPT_ARG_INT, &dbusschmitt, 0, "DBUS Schmitt Trigger Enable", "0/1"},
         {"cbus-drive", 0, POPT_ARG_INT, &cbusdrive, 0, "CBUS Drive Current Strength", "0-3"},
         {"cbus-slow", 0, POPT_ARG_INT, &cbusslow, 0, "CBUS Slew Rate Slow", "0/1"},
         {"cbus-schmitt", 0, POPT_ARG_INT, &cbusschmitt, 0, "CBUS Schmitt Trigger Enable", "0/1"},
         {"i2c-address", 0, POPT_ARG_INT, &i2cslave, 0, "I2C Slave Address", "0-127"},
         {"i2c-id1", 0, POPT_ARG_INT, &i2cid1, 0, "I2C Device ID Byte 1", "0-255"},
         {"i2c-id2", 0, POPT_ARG_INT, &i2cid2, 0, "I2C Device ID Byte 2", "0-255"},
         {"i2c-id3", 0, POPT_ARG_INT, &i2cid3, 0, "I2C Device ID Byte 3", "0-255"},
         {"debug", 0, POPT_ARG_NONE, &debug, 0, "Debug"},
         POPT_AUTOHELP {}
      };

      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp(optCon, "[filename]");

      int c;
      if ((c = poptGetNextOpt (optCon)) < -1)
         errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

      if (poptPeekArg (optCon))
      {
         poptPrintUsage (optCon, stderr, 0);
         return -1;
      }
   }

   struct ftdi_context *ftdi;

   memset (buf, 0, ELEN);
   if (!(ftdi = ftdi_new ()))
      errx (1, "Failed FTDI init");

   if (ftdi_usb_open_desc_index (ftdi, matchvid, matchpid, NULL, NULL, matchindex) < 0)
      errx (1, "Cannot find device - ARE YOU RUNNING THIS ON THE RIGHT MACHINE?");

   if (dtr == 2)
   {                            /* toggle */
      ftdi_setdtr (ftdi, 1);
      usleep (100000);
   } else if (dtr >= 0)
      ftdi_setdtr (ftdi, dtr);
   if (rts == 2)
   {                            /* toggle */
      ftdi_setrts (ftdi, 1);
      usleep (100000);
      ftdi_setrts (ftdi, 0);
      usleep (100000);
   } else if (rts >= 0)
      ftdi_setrts (ftdi, rts);
   if (dtr == 2)
   {
      ftdi_setdtr (ftdi, 0);
      usleep (100000);
   }
   unsigned char mask = 0;
   if (cbus0 >= 0)
      mask |= (1 << 0);
   if (cbus1 >= 0)
      mask |= (1 << 1);
   if (cbus2 >= 0)
      mask |= (1 << 2);
   if (cbus3 >= 0)
      mask |= (1 << 3);
   if (mask || cbusall >= 0 || reset)
   {                            /* Setting CBUS */
      unsigned char value = 0;
      if (cbusall > 0)
         value |= (0xF ^ mask);
      //All remaining bits
      if (cbusall >= 0)
         mask |= 0xF;
      //All bits
      if (cbus0 > 0)
         value |= (1 << 0);
      if (cbus1 > 0)
         value |= (1 << 1);
      if (cbus2 > 0)
         value |= (1 << 2);
      if (cbus3 > 0)
         value |= (1 << 3);
      if (ftdi_set_bitmode (ftdi, (mask << 4) | value, BITMODE_CBUS) < 0)
         errx (1, "Cannot set CBUS: %s", ftdi_get_error_string (ftdi));
      if (libusb_release_interface (ftdi->usb_dev, 0))
         errx (1, "Release failed");
      if (reset)
      {                         /* RTS reset */
         ftdi_setrts (ftdi, 1); /* makes low */
         usleep (100000);
         ftdi_setrts (ftdi, 0); /* makes high */
      }
   } else
   {                            /* EEPROM */
      if (ftdi_read_eeprom (ftdi))
         errx (1, "Cannot read EEPROM: %s", ftdi_get_error_string (ftdi));
      int elen = 0;
      if (ftdi_get_eeprom_value (ftdi, CHIP_SIZE, &elen))
         if (ftdi_read_eeprom (ftdi))
            errx (1, "Cannot read EEPROM: %s", ftdi_get_error_string (ftdi));
      if (debug)
         fprintf (stderr, "EEPROM size 0x%04X\n", elen);
      if (elen > ELEN)
         errx (1, "EEPROM too big");
      if (ftdi_get_eeprom_buf (ftdi, buf, elen))
         errx (1, "Cannot read EEPROM: %s", ftdi_get_error_string (ftdi));
      memcpy (was, buf, ELEN);

      /* Strings */
      setstring (0x07, manufacturer, "Manufacturer");
      setstring (0x08, description, "Description");
      setstring (0x09, serial, "Serial");

      /* General settings */
      setword (0x01, vid, "VID");
      setword (0x02, pid, "PID");
      setword (0x03, release, "Release");
      setbyte (0x1A, cbus0mode, "CBUS0 Mode");
      setbyte (0x1B, cbus1mode, "CBUS1 Mode");
      setbyte (0x1C, cbus2mode, "CBUS2 Mode");
      setbyte (0x1D, cbus3mode, "CBUS3 Mode");
      setbyte (0x1E, cbus4mode, "CBUS4 Mode");
      setbyte (0x1F, cbus5mode, "CBUS5 Mode");
      setbyte (0x20, cbus6mode, "CBUS6 Mode");
      setbit (0x00, 0, bcdenable, "BCD Enable");
      setbit (0x00, 1, forcepowerenable, "Force Power Enable");
      setbit (0x00, 2, deactivatesleep, "De-activate Sleep");
      setbit (0x00, 3, rs485echosuppress, "RS485 Echo Suppress");
      setbit (0x00, 4, extosc, "Ext OSC");
      setbit (0x00, 5, extoscfeedback, "Ext OSC Feedback Resistor Enable");
      setbit (0x00, 6, cbuspinsetforvbussense, "CBUS pin set for VBUS sense");
      setbit (0x00, 7, loadd2xxorvcpdrive, "Load D2XX or VCP Driver");
      setbit (0x08, 5, enableusbremote, "Enable USB Remote wakeup");
      setbit (0x08, 6, selfpowered, "Self Powered");
      setbyte (0x09, maxpower, "Max Power Value");
      setbit (0x0A, 2, suspendpulldown, "USB suspend pull down enable");
      setbit (0x0A, 3, enableserialnumber, "Enable/Disable USB Serial Number");
      setbit (0x0A, 4, ft1249cpol, "FT1248 Clock polarity");
      setbit (0x0A, 5, ft1249bord, "FT1248 Bit Order");
      setbit (0x0A, 6, ft1249flow, "FT1248 Flow Control Enable");
      setbit (0x0A, 7, disableschmitt, "Disable I2C Schmitt");
      setbit (0x0B, 0, inverttxd, "Invert TXD");
      setbit (0x0B, 1, invertrxd, "Invert RXD");
      setbit (0x0B, 2, invertrts, "Invert RTS");
      setbit (0x0B, 3, invertcts, "Invert CTS");
      setbit (0x0B, 4, invertdtr, "Invert DTR");
      setbit (0x0B, 5, invertdsr, "Invert DSR");
      setbit (0x0B, 6, invertdcd, "Invert DCD");
      setbit (0x0B, 7, invertri, "Invert RI");
      setbit2 (0x0C, 0, dbusdrive, "DBUS Drive Current Strength");
      setbit (0x0C, 2, dbusslow, "DBUS Slew Rate Slow");
      setbit (0x0C, 3, dbusschmitt, "DBUS Schmitt Trigger Enable");
      setbit2 (0x0C, 4, cbusdrive, "CBUS Drive Current Strength");
      setbit (0x0C, 6, cbusslow, "CBUS Slew Rate Slow");
      setbit (0x0C, 7, cbusschmitt, "CBUS Schmitt Trigger Enable");
      setword (0x0A, i2cslave, "I2C Slave Address");
      setbyte (0x16, i2cid1, "I2C Device ID Byte 1");
      setbyte (0x17, i2cid2, "I2C Device ID Byte 2");
      setbyte (0x18, i2cid3, "I2C Device ID Byte 3");

      checksum ();

      for (int a = 0; a < ELEN; a += 2)
         if (was[a] != buf[a] || was[a + 1] != buf[a + 1])
         {
            if (libusb_control_transfer (ftdi->usb_dev, FTDI_DEVICE_OUT_REQTYPE,
                                         SIO_WRITE_EEPROM_REQUEST, getword (a / 2), a / 2, NULL, 0, ftdi->usb_write_timeout))
               errx (1, "Write failed at 0x%04X: %s", a, ftdi_get_error_string (ftdi));
            reset = 1;
         }
      if (libusb_release_interface (ftdi->usb_dev, 0))
         errx (1, "Release failed");
      if (reset)
         libusb_reset_device (ftdi->usb_dev);
   }
   //libusb_close(ftdi->usb_dev);       /* why does this seg fault? */
   //ftdi_free(ftdi);

   poptFreeContext (optCon);
   return 0;
}
