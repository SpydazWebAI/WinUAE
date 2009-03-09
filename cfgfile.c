 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Config file handling
  * This still needs some thought before it's complete...
  *
  * Copyright 1998 Brian King, Bernd Schmidt
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>

#include "options.h"
#include "uae.h"
#include "audio.h"
#include "autoconf.h"
#include "events.h"
#include "custom.h"
#include "inputdevice.h"
#include "gfxfilter.h"
#include "savestate.h"
#include "memory.h"
#include "gui.h"
#include "newcpu.h"
#include "zfile.h"
#include "filesys.h"
#include "fsdb.h"
#include "disk.h"
#include "statusline.h"

static int config_newfilesystem;
static struct strlist *temp_lines;
static struct zfile *default_file;
static int uaeconfig;
static int unicode_config = 0;

/* @@@ need to get rid of this... just cut part of the manual and print that
 * as a help text.  */
struct cfg_lines
{
    const TCHAR *config_label, *config_help;
};

static const struct cfg_lines opttable[] =
{
    {L"help", L"Prints this help" },
    {L"config_description", L"" },
    {L"config_info", L"" },
    {L"use_gui", L"Enable the GUI?  If no, then goes straight to emulator" },
    {L"use_debugger", L"Enable the debugger?" },
    {L"cpu_speed", L"can be max, real, or a number between 1 and 20" },
    {L"cpu_model", L"Can be 68000, 68010, 68020, 68030, 68040, 68060" },
    {L"fpu_model", L"Can be 68881, 68882, 68040, 68060" },
    {L"cpu_compatible", L"yes enables compatibility-mode" },
    {L"cpu_24bit_addressing", L"must be set to 'no' in order for Z3mem or P96mem to work" },
    {L"autoconfig", L"yes = add filesystems and extra ram" },
    {L"log_illegal_mem", L"print illegal memory access by Amiga software?" },
    {L"fastmem_size", L"Size in megabytes of fast-memory" },
    {L"chipmem_size", L"Size in megabytes of chip-memory" },
    {L"bogomem_size", L"Size in megabytes of bogo-memory at 0xC00000" },
    {L"a3000mem_size", L"Size in megabytes of A3000 memory" },
    {L"gfxcard_size", L"Size in megabytes of Picasso96 graphics-card memory" },
    {L"z3mem_size", L"Size in megabytes of Zorro-III expansion memory" },
    {L"gfx_test_speed", L"Test graphics speed?" },
    {L"gfx_framerate", L"Print every nth frame" },
    {L"gfx_width", L"Screen width" },
    {L"gfx_height", L"Screen height" },
    {L"gfx_refreshrate", L"Fullscreen refresh rate" },
    {L"gfx_vsync", L"Sync screen refresh to refresh rate" },
    {L"gfx_lores", L"Treat display as lo-res?" },
    {L"gfx_linemode", L"Can be none, double, or scanlines" },
    {L"gfx_fullscreen_amiga", L"Amiga screens are fullscreen?" },
    {L"gfx_fullscreen_picasso", L"Picasso screens are fullscreen?" },
    {L"gfx_center_horizontal", L"Center display horizontally?" },
    {L"gfx_center_vertical", L"Center display vertically?" },
    {L"gfx_colour_mode", L"" },
    {L"32bit_blits", L"Enable 32 bit blitter emulation" },
    {L"immediate_blits", L"Perform blits immediately" },
    {L"show_leds", L"LED display" },
    {L"keyboard_leds", L"Keyboard LEDs" },
    {L"gfxlib_replacement", L"Use graphics.library replacement?" },
    {L"sound_output", L"" },
    {L"sound_frequency", L"" },
    {L"sound_bits", L"" },
    {L"sound_channels", L"" },
    {L"sound_max_buff", L"" },
    {L"comp_trustbyte", L"How to access bytes in compiler (direct/indirect/indirectKS/afterPic" },
    {L"comp_trustword", L"How to access words in compiler (direct/indirect/indirectKS/afterPic" },
    {L"comp_trustlong", L"How to access longs in compiler (direct/indirect/indirectKS/afterPic" },
    {L"comp_nf", L"Whether to optimize away flag generation where possible" },
    {L"comp_fpu", L"Whether to provide JIT FPU emulation" },
    {L"compforcesettings", L"Whether to force the JIT compiler settings" },
    {L"cachesize", L"How many MB to use to buffer translated instructions"},
    {L"override_dga_address",L"Address from which to map the frame buffer (upper 16 bits) (DANGEROUS!)"},
    {L"avoid_cmov", L"Set to yes on machines that lack the CMOV instruction" },
    {L"avoid_dga", L"Set to yes if the use of DGA extension creates problems" },
    {L"avoid_vid", L"Set to yes if the use of the Vidmode extension creates problems" },
    {L"parallel_on_demand", L"" },
    {L"serial_on_demand", L"" },
    {L"scsi", L"scsi.device emulation" },
    {L"joyport0", L"" },
    {L"joyport1", L"" },
    {L"pci_devices", L"List of PCI devices to make visible to the emulated Amiga" },
    {L"kickstart_rom_file", L"Kickstart ROM image, (C) Copyright Amiga, Inc." },
    {L"kickstart_ext_rom_file", L"Extended Kickstart ROM image, (C) Copyright Amiga, Inc." },
    {L"kickstart_key_file", L"Key-file for encrypted ROM images (from Cloanto's Amiga Forever)" },
    {L"flash_ram_file", L"Flash/battery backed RAM image file." },
    {L"cart_file", L"Freezer cartridge ROM image file." },
    {L"floppy0", L"Diskfile for drive 0" },
    {L"floppy1", L"Diskfile for drive 1" },
    {L"floppy2", L"Diskfile for drive 2" },
    {L"floppy3", L"Diskfile for drive 3" },
    {L"hardfile", L"access,sectors, surfaces, reserved, blocksize, path format" },
    {L"filesystem", L"access,'Amiga volume-name':'host directory path' - where 'access' can be 'read-only' or 'read-write'" },
    {L"catweasel", L"Catweasel board io base address" }
};

static const TCHAR *guimode1[] = { L"no", L"yes", L"nowait", 0 };
static const TCHAR *guimode2[] = { L"false", L"true", L"nowait", 0 };
static const TCHAR *guimode3[] = { L"0", L"1", L"nowait", 0 };
static const TCHAR *csmode[] = { L"ocs", L"ecs_agnus", L"ecs_denise", L"ecs", L"aga", 0 };
static const TCHAR *linemode1[] = { L"none", L"double", L"scanlines", 0 };
static const TCHAR *linemode2[] = { L"n", L"d", L"s", 0 };
static const TCHAR *speedmode[] = { L"max", L"real", 0 };
static const TCHAR *colormode1[] = { L"8bit", L"15bit", L"16bit", L"8bit_dither", L"4bit_dither", L"32bit", 0 };
static const TCHAR *colormode2[] = { L"8", L"15", L"16", L"8d", L"4d", L"32", 0 };
static const TCHAR *soundmode1[] = { L"none", L"interrupts", L"normal", L"exact", 0 };
static const TCHAR *soundmode2[] = { L"none", L"interrupts", L"good", L"best", 0 };
static const TCHAR *centermode1[] = { L"none", L"simple", L"smart", 0 };
static const TCHAR *centermode2[] = { L"false", L"true", L"smart", 0 };
static const TCHAR *stereomode[] = { L"mono", L"stereo", L"clonedstereo", L"4ch", L"clonedstereo6ch", L"6ch", L"mixed", 0 };
static const TCHAR *interpolmode[] = { L"none", L"anti", L"sinc", L"rh", L"crux", 0 };
static const TCHAR *collmode[] = { L"none", L"sprites", L"playfields", L"full", 0 };
static const TCHAR *compmode[] = { L"direct", L"indirect", L"indirectKS", L"afterPic", 0 };
static const TCHAR *flushmode[] = { L"soft", L"hard", 0 };
static const TCHAR *kbleds[] = { L"none", L"POWER", L"DF0", L"DF1", L"DF2", L"DF3", L"HD", L"CD", 0 };
static const TCHAR *onscreenleds[] = { L"false", L"true", L"rtg", L"both", 0 };
static const TCHAR *soundfiltermode1[] = { L"off", L"emulated", L"on", 0 };
static const TCHAR *soundfiltermode2[] = { L"standard", L"enhanced", 0 };
static const TCHAR *lorestype1[] = { L"lores", L"hires", L"superhires" };
static const TCHAR *lorestype2[] = { L"true", L"false" };
static const TCHAR *loresmode[] = { L"normal", L"filtered", 0 };
#ifdef GFXFILTER
static const TCHAR *filtermode1[] = { L"no_16", L"bilinear_16", L"no_32", L"bilinear_32", 0 };
static const TCHAR *filtermode2[] = { L"0x", L"1x", L"2x", L"3x", L"4x", 0 };
#endif
static const TCHAR *cartsmode[] = { L"none", L"hrtmon", 0 };
static const TCHAR *idemode[] = { L"none", L"a600/a1200", L"a4000", 0 };
static const TCHAR *rtctype[] = { L"none", L"MSM6242B", L"RP5C01A", 0 };
static const TCHAR *ciaatodmode[] = { L"vblank", L"50hz", L"60hz", 0 };
static const TCHAR *ksmirrortype[] = { L"none", L"e0", L"a8+e0", 0 };
static const TCHAR *cscompa[] = {
    L"-", L"Generic", L"CDTV", L"CD32", L"A500", L"A500+", L"A600",
    L"A1000", L"A1200", L"A2000", L"A3000", L"A3000T", L"A4000", L"A4000T", 0
};
/* 3-state boolean! */
static const TCHAR *fullmodes[] = { L"false", L"true", /* "FILE_NOT_FOUND", */ L"fullwindow", 0 };
/* bleh for compatibility */
static const TCHAR *scsimode[] = { L"false", L"true", L"scsi", 0 };
static const TCHAR *maxhoriz[] = { L"lores", L"hires", L"superhires", 0 };
static const TCHAR *maxvert[] = { L"nointerlace", L"interlace", 0 };
static const TCHAR *abspointers[] = { L"none", L"mousehack", L"tablet", 0 };
static const TCHAR *magiccursors[] = { L"both", L"native", L"host", 0 };
static const TCHAR *autoscale[] = { L"none", L"scale", L"resize", 0 };

static const TCHAR *obsolete[] = {
    L"accuracy", L"gfx_opengl", L"gfx_32bit_blits", L"32bit_blits",
    L"gfx_immediate_blits", L"gfx_ntsc", L"win32", L"gfx_filter_bits",
    L"sound_pri_cutoff", L"sound_pri_time", L"sound_min_buff", L"sound_bits",
    L"gfx_test_speed", L"gfxlib_replacement", L"enforcer", L"catweasel_io",
    L"kickstart_key_file", L"fast_copper", L"sound_adjust",
    L"serial_hardware_dtrdsr", L"gfx_filter_upscale",
    L"gfx_correct_aspect",
    NULL
};

#define UNEXPANDED L"$(FILE_PATH)"

static void trimws (TCHAR *s)
{
    /* Delete trailing whitespace.  */
    int len = _tcslen (s);
    while (len > 0 && _tcscspn (s + len - 1, L"\t \r\n") == 0)
        s[--len] = '\0';
}

static int match_string (const TCHAR *table[], const TCHAR *str)
{
    int i;
    for (i = 0; table[i] != 0; i++)
	if (strcasecmp (table[i], str) == 0)
	    return i;
    return -1;
}

TCHAR *cfgfile_subst_path (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
    /* @@@ use strcasecmp for some targets.  */
    if (_tcslen (path) > 0 && _tcsncmp (file, path, _tcslen (path)) == 0) {
	int l;
	TCHAR *p = xmalloc ((_tcslen (file) + _tcslen (subst) + 2) * sizeof (TCHAR));
	_tcscpy (p, subst);
	l = _tcslen (p);
	while (l > 0 && p[l - 1] == '/')
	    p[--l] = '\0';
	l = _tcslen (path);
	while (file[l] == '/')
	    l++;
	_tcscat (p, L"/"); _tcscat (p, file + l);
	return p;
    }
    return my_strdup (file);
}

static int isdefault (const TCHAR *s)
{
    TCHAR tmp[MAX_DPATH];
    if (!default_file || uaeconfig)
	return 0;
    zfile_fseek (default_file, 0, SEEK_SET);
    while (zfile_fgets (tmp, sizeof tmp / sizeof (TCHAR), default_file)) {
	if (!_tcscmp (tmp, s))
	    return 1;
    }
    return 0;
}

static size_t cfg_write (void *b, struct zfile *z)
{
    size_t v;
    if (unicode_config) {
	v = zfile_fwrite (b, _tcslen (b), sizeof (TCHAR), z);
    } else {
	char *s = ua (b);
	v = zfile_fwrite (s, strlen(s), 1, z);
	xfree (s);
    }
    return v;
}


void cfgfile_write (struct zfile *f, TCHAR *format,...)
{
    va_list parms;
    TCHAR tmp[CONFIG_BLEN];

    va_start (parms, format);
    _vstprintf (tmp, format, parms);
    cfg_write (tmp, f);
    va_end (parms);
}

void cfgfile_dwrite (struct zfile *f, TCHAR *format,...)
{
    va_list parms;
    TCHAR tmp[CONFIG_BLEN];

    va_start (parms, format);
    _vstprintf (tmp, format, parms);
    if (!isdefault (tmp))
	cfg_write (tmp, f);
    va_end (parms);
}

void cfgfile_target_write (struct zfile *f, TCHAR *format,...)
{
    va_list parms;
    TCHAR tmp[CONFIG_BLEN];

    va_start (parms, format);
    _vstprintf (tmp + _tcslen (TARGET_NAME) + 1, format, parms);
    memcpy (tmp, TARGET_NAME, _tcslen (TARGET_NAME) * sizeof (TCHAR));
    tmp[_tcslen (TARGET_NAME)] = '.';
    cfg_write (tmp, f);
    va_end (parms);
}

void cfgfile_target_dwrite (struct zfile *f, TCHAR *format,...)
{
    va_list parms;
    TCHAR tmp[CONFIG_BLEN];

    va_start (parms, format);
    _vstprintf (tmp + _tcslen (TARGET_NAME) + 1, format, parms);
    memcpy (tmp, TARGET_NAME, _tcslen (TARGET_NAME) * sizeof (TCHAR));
    tmp[_tcslen (TARGET_NAME)] = '.';
    if (!isdefault (tmp))
	cfg_write (tmp, f);
    va_end (parms);
}

static void write_filesys_config (struct uae_prefs *p, const TCHAR *unexpanded,
				  const TCHAR *default_path, struct zfile *f)
{
    int i;
    TCHAR tmp[MAX_DPATH], tmp2[MAX_DPATH];
    TCHAR *hdcontrollers[] = { L"uae",
	L"ide0", L"ide1", L"ide2", L"ide3",
	L"scsi0", L"scsi1", L"scsi2", L"scsi3", L"scsi4", L"scsi5", L"scsi6",
	L"scsram", L"scside" }; /* scsram = smart card sram = pcmcia sram card */

    for (i = 0; i < p->mountitems; i++) {
	struct uaedev_config_info *uci = &p->mountconfig[i];
	TCHAR *str;
        int bp = uci->bootpri;

	if (!uci->autoboot)
	    bp = -128;
	if (uci->donotmount)
	    bp = -129;
	str = cfgfile_subst_path (default_path, unexpanded, uci->rootdir);
	if (!uci->ishdf) {
	    _stprintf (tmp, L"%s,%s:%s:%s,%d\n", uci->readonly ? L"ro" : L"rw",
		uci->devname ? uci->devname : L"", uci->volname, str, bp);
	    _stprintf (tmp2, L"filesystem2=%s", tmp);
	    zfile_fputs (f, tmp2);
#if 0
	    _stprintf (tmp2, L"filesystem=%s,%s:%s\n", uci->readonly ? L"ro" : L"rw",
		uci->volname, str);
	    zfile_fputs (f, tmp2);
#endif
	} else {
	    _stprintf (tmp, L"%s,%s:%s,%d,%d,%d,%d,%d,%s,%s\n",
		     uci->readonly ? L"ro" : L"rw",
		     uci->devname ? uci->devname : L"", str,
		     uci->sectors, uci->surfaces, uci->reserved, uci->blocksize,
		     bp, uci->filesys ? uci->filesys : L"", hdcontrollers[uci->controller]);
	    _stprintf (tmp2, L"hardfile2=%s", tmp);
	    zfile_fputs (f, tmp2);
#if 0
	    _stprintf (tmp2, L"hardfile=%s,%d,%d,%d,%d,%s\n",
		     uci->readonly ? "ro" : "rw", uci->sectors,
		     uci->surfaces, uci->reserved, uci->blocksize, str);
	    zfile_fputs (f, tmp2);
#endif
	}
	_stprintf (tmp2, L"uaehf%d=%s,%s", i, uci->ishdf ? L"hdf" : L"dir", tmp);
	zfile_fputs (f, tmp2);
	xfree (str);
    }
}

static void write_compatibility_cpu (struct zfile *f, struct uae_prefs *p)
{
    TCHAR tmp[100];
    int model;

    model = p->cpu_model;
    if (model == 68030)
	model = 68020;
    if (model == 68060)
	model = 68040;
    if (p->address_space_24 && model == 68020)
	_tcscpy (tmp, L"68ec020");
    else
	_stprintf (tmp, L"%d", model);
    if (model == 68020 && (p->fpu_model == 68881 || p->fpu_model == 68882))
	_tcscat (tmp, L"/68881");
    cfgfile_write (f, L"cpu_type=%s\n", tmp);
}

void cfgfile_save_options (struct zfile *f, struct uae_prefs *p, int type)
{
    struct strlist *sl;
    TCHAR *str;
    int i;

    cfgfile_write (f, L"config_description=%s\n", p->description);
    cfgfile_write (f, L"config_hardware=%s\n", (type & CONFIG_TYPE_HARDWARE) ? L"true" : L"false");
    cfgfile_write (f, L"config_host=%s\n", (type & CONFIG_TYPE_HOST) ? L"true" : L"false");
    if (p->info[0])
	cfgfile_write (f, L"config_info=%s\n", p->info);
    cfgfile_write (f, L"config_version=%d.%d.%d\n", UAEMAJOR, UAEMINOR, UAESUBREV);
    cfgfile_write (f, L"config_hardware_path=%s\n", p->config_hardware_path);
    cfgfile_write (f, L"config_host_path=%s\n", p->config_host_path);

    for (sl = p->all_lines; sl; sl = sl->next) {
	if (sl->unknown)
	    cfgfile_write (f, L"%s=%s\n", sl->option, sl->value);
    }

    cfgfile_write (f, L"%s.rom_path=%s\n", TARGET_NAME, p->path_rom);
    cfgfile_write (f, L"%s.floppy_path=%s\n", TARGET_NAME, p->path_floppy);
    cfgfile_write (f, L"%s.hardfile_path=%s\n", TARGET_NAME, p->path_hardfile);

    cfgfile_write (f, L"; host-specific\n");

    target_save_options (f, p);

    cfgfile_write (f, L"; common\n");

    cfgfile_write (f, L"use_gui=%s\n", guimode1[p->start_gui]);
    cfgfile_write (f, L"use_debugger=%s\n", p->start_debugger ? L"true" : L"false");
    str = cfgfile_subst_path (p->path_rom, UNEXPANDED, p->romfile);
    cfgfile_write (f, L"kickstart_rom_file=%s\n", str);
    free (str);
    if (p->romident[0])
	cfgfile_dwrite (f, L"kickstart_rom=%s\n", p->romident);
    str = cfgfile_subst_path (p->path_rom, UNEXPANDED, p->romextfile);
    cfgfile_write (f, L"kickstart_ext_rom_file=%s\n", str);
    free (str);
    if (p->romextident[0])
	cfgfile_write (f, L"kickstart_ext_rom=%s\n", p->romextident);
    str = cfgfile_subst_path (p->path_rom, UNEXPANDED, p->flashfile);
    cfgfile_write (f, L"flash_file=%s\n", str);
    free (str);
    str = cfgfile_subst_path (p->path_rom, UNEXPANDED, p->cartfile);
    cfgfile_write (f, L"cart_file=%s\n", str);
    free (str);
    if (p->cartident[0])
	cfgfile_write (f, L"cart=%s\n", p->cartident);
    //cfgfile_write (f, L"cart_internal=%s\n", cartsmode[p->cart_internal]);
    if (p->amaxromfile[0]) {
	str = cfgfile_subst_path (p->path_rom, UNEXPANDED, p->amaxromfile);
	cfgfile_write (f, L"amax_rom_file=%s\n", str);
	free (str);
    }

    cfgfile_write (f, L"kickshifter=%s\n", p->kickshifter ? L"true" : L"false");

    p->nr_floppies = 4;
    for (i = 0; i < 4; i++) {
	str = cfgfile_subst_path (p->path_floppy, UNEXPANDED, p->df[i]);
	cfgfile_write (f, L"floppy%d=%s\n", i, str);
	free (str);
	cfgfile_dwrite (f, L"floppy%dtype=%d\n", i, p->dfxtype[i]);
	cfgfile_dwrite (f, L"floppy%dsound=%d\n", i, p->dfxclick[i]);
	if (p->dfxclick[i] < 0 && p->dfxclickexternal[i][0])
	    cfgfile_dwrite (f, L"floppy%dsoundext=%s\n", i, p->dfxclickexternal[i]);
	if (p->dfxtype[i] < 0 && p->nr_floppies > i)
	    p->nr_floppies = i;
    }
    for (i = 0; i < MAX_SPARE_DRIVES; i++) {
	if (p->dfxlist[i][0])
	    cfgfile_dwrite (f, L"diskimage%d=%s\n", i, p->dfxlist[i]);
    }

    cfgfile_write (f, L"nr_floppies=%d\n", p->nr_floppies);
    cfgfile_write (f, L"floppy_speed=%d\n", p->floppy_speed);
    cfgfile_write (f, L"floppy_volume=%d\n", p->dfxclickvolume);
    cfgfile_write (f, L"parallel_on_demand=%s\n", p->parallel_demand ? L"true" : L"false");
    cfgfile_write (f, L"serial_on_demand=%s\n", p->serial_demand ? L"true" : L"false");
    cfgfile_write (f, L"serial_hardware_ctsrts=%s\n", p->serial_hwctsrts ? L"true" : L"false");
    cfgfile_write (f, L"serial_direct=%s\n", p->serial_direct ? L"true" : L"false");
    cfgfile_write (f, L"scsi=%s\n", scsimode[p->scsi]);
    cfgfile_write (f, L"uaeserial=%s\n", p->uaeserial ? L"true" : L"false");
    cfgfile_write (f, L"sana2=%s\n", p->sana2 ? L"true" : L"false");

    cfgfile_write (f, L"sound_output=%s\n", soundmode1[p->produce_sound]);
    cfgfile_write (f, L"sound_channels=%s\n", stereomode[p->sound_stereo]);
    cfgfile_write (f, L"sound_stereo_separation=%d\n", p->sound_stereo_separation);
    cfgfile_write (f, L"sound_stereo_mixing_delay=%d\n", p->sound_mixed_stereo_delay >= 0 ? p->sound_mixed_stereo_delay : 0);
    cfgfile_write (f, L"sound_max_buff=%d\n", p->sound_maxbsiz);
    cfgfile_write (f, L"sound_frequency=%d\n", p->sound_freq);
    cfgfile_write (f, L"sound_latency=%d\n", p->sound_latency);
    cfgfile_write (f, L"sound_interpol=%s\n", interpolmode[p->sound_interpol]);
    cfgfile_write (f, L"sound_filter=%s\n", soundfiltermode1[p->sound_filter]);
    cfgfile_write (f, L"sound_filter_type=%s\n", soundfiltermode2[p->sound_filter_type]);
    cfgfile_write (f, L"sound_volume=%d\n", p->sound_volume);
    cfgfile_write (f, L"sound_auto=%s\n", p->sound_auto ? L"yes" : L"no");
    cfgfile_write (f, L"sound_stereo_swap_paula=%s\n", p->sound_stereo_swap_paula ? L"yes" : L"no");
    cfgfile_write (f, L"sound_stereo_swap_ahi=%s\n", p->sound_stereo_swap_ahi ? L"yes" : L"no");

    cfgfile_write (f, L"comp_trustbyte=%s\n", compmode[p->comptrustbyte]);
    cfgfile_write (f, L"comp_trustword=%s\n", compmode[p->comptrustword]);
    cfgfile_write (f, L"comp_trustlong=%s\n", compmode[p->comptrustlong]);
    cfgfile_write (f, L"comp_trustnaddr=%s\n", compmode[p->comptrustnaddr]);
    cfgfile_write (f, L"comp_nf=%s\n", p->compnf ? L"true" : L"false");
    cfgfile_write (f, L"comp_constjump=%s\n", p->comp_constjump ? L"true" : L"false");
    cfgfile_write (f, L"comp_oldsegv=%s\n", p->comp_oldsegv ? L"true" : L"false");

    cfgfile_write (f, L"comp_flushmode=%s\n", flushmode[p->comp_hardflush]);
    cfgfile_write (f, L"compfpu=%s\n", p->compfpu ? L"true" : L"false");
    cfgfile_write (f, L"fpu_strict=%s\n", p->fpu_strict ? L"true" : L"false");
    cfgfile_write (f, L"comp_midopt=%s\n", p->comp_midopt ? L"true" : L"false");
    cfgfile_write (f, L"comp_lowopt=%s\n", p->comp_lowopt ? L"true" : L"false");
    cfgfile_write (f, L"avoid_cmov=%s\n", p->avoid_cmov ? L"true" : L"false" );
    cfgfile_write (f, L"avoid_dga=%s\n", p->avoid_dga ? L"true" : L"false" );
    cfgfile_write (f, L"avoid_vid=%s\n", p->avoid_vid ? L"true" : L"false" );
    cfgfile_write (f, L"cachesize=%d\n", p->cachesize);
    if (p->override_dga_address)
	cfgfile_write (f, L"override_dga_address=0x%08x\n", p->override_dga_address);

    for (i = 0; i < 2; i++) {
	struct jport *jp = &p->jports[i];
	int v = jp->id;
	TCHAR tmp1[MAX_DPATH], tmp2[MAX_DPATH];
	if (v < 0) {
	    _tcscpy (tmp2, L"none");
	} else if (v < JSEM_JOYS) {
	    _stprintf (tmp2, L"kbd%d", v + 1);
	} else if (v < JSEM_MICE) {
	    _stprintf (tmp2, L"joy%d", v - JSEM_JOYS);
	} else {
	    _tcscpy (tmp2, L"mouse");
	    if (v - JSEM_MICE > 0)
		_stprintf (tmp2, L"mouse%d", v - JSEM_MICE);
	}
	_stprintf (tmp1, L"joyport%d=%s\n", i, tmp2);
	cfgfile_write (f, tmp1);
	if (jp->name) {
	    _stprintf (tmp1, L"joyportfriendlyname%d=%s\n", i, jp->name);
	    cfgfile_write (f, tmp1);
	}
	if (jp->configname) {
	    _stprintf (tmp1, L"joyportname%d=%s\n", i, jp->configname);
	    cfgfile_write (f, tmp1);
	}
    }

    cfgfile_write (f, L"bsdsocket_emu=%s\n", p->socket_emu ? L"true" : L"false");

    cfgfile_write (f, L"synchronize_clock=%s\n", p->tod_hack ? L"yes" : L"no");
    cfgfile_write (f, L"maprom=0x%x\n", p->maprom);
    cfgfile_write (f, L"parallel_postscript_emulation=%s\n", p->parallel_postscript_emulation ? L"yes" : L"no");
    cfgfile_write (f, L"parallel_postscript_detection=%s\n", p->parallel_postscript_detection ? L"yes" : L"no");
    cfgfile_write (f, L"ghostscript_parameters=%s\n", p->ghostscript_parameters);
    cfgfile_write (f, L"parallel_autoflush=%d\n", p->parallel_autoflush_time);
    cfgfile_dwrite (f, L"uae_hide=%d\n", p->uae_hide);
    cfgfile_dwrite (f, L"magic_mouse=%s\n", p->input_magic_mouse ? L"true" : L"false");
    cfgfile_dwrite (f, L"magic_mousecursor=%s\n", magiccursors[p->input_magic_mouse_cursor]);
    cfgfile_dwrite (f, L"absolute_mouse=%s\n", abspointers[p->input_tablet]);

    cfgfile_write (f, L"gfx_display=%d\n", p->gfx_display);
    cfgfile_write (f, L"gfx_display_name=%s\n", p->gfx_display_name);
    cfgfile_write (f, L"gfx_framerate=%d\n", p->gfx_framerate);
    cfgfile_write (f, L"gfx_width=%d\n", p->gfx_size_win.width); /* compatibility with old versions */
    cfgfile_write (f, L"gfx_height=%d\n", p->gfx_size_win.height); /* compatibility with old versions */
    cfgfile_write (f, L"gfx_top_windowed=%d\n", p->gfx_size_win.x);
    cfgfile_write (f, L"gfx_left_windowed=%d\n", p->gfx_size_win.y);
    cfgfile_write (f, L"gfx_width_windowed=%d\n", p->gfx_size_win.width);
    cfgfile_write (f, L"gfx_height_windowed=%d\n", p->gfx_size_win.height);
    cfgfile_write (f, L"gfx_width_fullscreen=%d\n", p->gfx_size_fs.width);
    cfgfile_write (f, L"gfx_height_fullscreen=%d\n", p->gfx_size_fs.height);
    cfgfile_write (f, L"gfx_refreshrate=%d\n", p->gfx_refreshrate);
    cfgfile_write (f, L"gfx_autoresolution=%d\n", p->gfx_autoresolution);
    cfgfile_write (f, L"gfx_vsync=%s\n", p->gfx_avsync ? L"true" : L"false");
    cfgfile_write (f, L"gfx_vsync_picasso=%s\n", p->gfx_pvsync ? L"true" : L"false");
    cfgfile_write (f, L"gfx_lores=%s\n", p->gfx_resolution == 0 ? L"true" : L"false");
    cfgfile_write (f, L"gfx_resolution=%s\n", lorestype1[p->gfx_resolution]);
    cfgfile_write (f, L"gfx_lores_mode=%s\n", loresmode[p->gfx_lores_mode]);
    cfgfile_write (f, L"gfx_flickerfixer=%s\n", p->gfx_scandoubler ? L"true" : L"false");
    cfgfile_write (f, L"gfx_linemode=%s\n", linemode1[p->gfx_linedbl]);
    cfgfile_write (f, L"gfx_fullscreen_amiga=%s\n", fullmodes[p->gfx_afullscreen]);
    cfgfile_write (f, L"gfx_fullscreen_picasso=%s\n", fullmodes[p->gfx_pfullscreen]);
    cfgfile_write (f, L"gfx_center_horizontal=%s\n", centermode1[p->gfx_xcenter]);
    cfgfile_write (f, L"gfx_center_vertical=%s\n", centermode1[p->gfx_ycenter]);
    cfgfile_write (f, L"gfx_colour_mode=%s\n", colormode1[p->color_mode]);
    cfgfile_write (f, L"gfx_blacker_than_black=%s\n", p->gfx_blackerthanblack ? L"true" : L"false");

#ifdef GFXFILTER
    if (p->gfx_filter > 0) {
	int i = 0;
	struct uae_filter *uf;
	while (uaefilters[i].name) {
	    uf = &uaefilters[i];
	    if (uf->type == p->gfx_filter) {
		if (p->gfx_filtershader[0])
		    cfgfile_dwrite (f, L"gfx_filter=%s:%s\n", uf->cfgname, p->gfx_filtershader);
		else
		    cfgfile_dwrite (f, L"gfx_filter=%s\n", uf->cfgname);
		if (uf->type == p->gfx_filter) {
		    if (uf->x[0]) {
			cfgfile_dwrite (f, L"gfx_filter_mode=%s\n", filtermode1[p->gfx_filter_filtermode]);
		    } else {
			int mt[4], i = 0;
			if (uf->x[1])
			    mt[i++] = 1;
			if (uf->x[2])
			    mt[i++] = 2;
			if (uf->x[3])
			    mt[i++] = 3;
			if (uf->x[4])
			    mt[i++] = 4;
			cfgfile_dwrite (f, L"gfx_filter_mode=%dx\n", mt[p->gfx_filter_filtermode]);
		    }
		}
	    }
	    i++;
	}
    } else {
	cfgfile_dwrite (f, L"gfx_filter=no\n");
    }

    cfgfile_dwrite (f, L"gfx_filter_vert_zoom=%d\n", p->gfx_filter_vert_zoom);
    cfgfile_dwrite (f, L"gfx_filter_horiz_zoom=%d\n", p->gfx_filter_horiz_zoom);
    cfgfile_dwrite (f, L"gfx_filter_vert_zoom_mult=%d\n", p->gfx_filter_vert_zoom_mult);
    cfgfile_dwrite (f, L"gfx_filter_horiz_zoom_mult=%d\n", p->gfx_filter_horiz_zoom_mult);
    cfgfile_dwrite (f, L"gfx_filter_vert_offset=%d\n", p->gfx_filter_vert_offset);
    cfgfile_dwrite (f, L"gfx_filter_horiz_offset=%d\n", p->gfx_filter_horiz_offset);
    cfgfile_dwrite (f, L"gfx_filter_scanlines=%d\n", p->gfx_filter_scanlines);
    cfgfile_dwrite (f, L"gfx_filter_scanlinelevel=%d\n", p->gfx_filter_scanlinelevel);
    cfgfile_dwrite (f, L"gfx_filter_scanlineratio=%d\n", p->gfx_filter_scanlineratio);
    cfgfile_dwrite (f, L"gfx_filter_luminance=%d\n", p->gfx_filter_luminance);
    cfgfile_dwrite (f, L"gfx_filter_contrast=%d\n", p->gfx_filter_contrast);
    cfgfile_dwrite (f, L"gfx_filter_saturation=%d\n", p->gfx_filter_saturation);
    cfgfile_dwrite (f, L"gfx_filter_gamma=%d\n", p->gfx_filter_gamma);
    cfgfile_dwrite (f, L"gfx_filter_blur=%d\n", p->gfx_filter_blur);
    cfgfile_dwrite (f, L"gfx_filter_noise=%d\n", p->gfx_filter_noise);
    cfgfile_dwrite (f, L"gfx_filter_keep_aspect=%s\n", p->gfx_filter_keep_aspect ? L"true" : L"false");
    cfgfile_dwrite (f, L"gfx_filter_autoscale=%s\n", autoscale[p->gfx_filter_autoscale]);
    cfgfile_dwrite (f, L"gfx_filter_aspect_ratio=%d:%d\n",
	p->gfx_filter_aspect >= 0 ? (p->gfx_filter_aspect >> 8) : -1,
	p->gfx_filter_aspect >= 0 ? (p->gfx_filter_aspect & 0xff) : -1);
    cfgfile_dwrite (f, L"gfx_luminance=%d\n", p->gfx_luminance);
    cfgfile_dwrite (f, L"gfx_contrast=%d\n", p->gfx_contrast);
    cfgfile_dwrite (f, L"gfx_gamma=%d\n", p->gfx_gamma);
#endif

    cfgfile_write (f, L"immediate_blits=%s\n", p->immediate_blits ? L"true" : L"false");
    cfgfile_write (f, L"ntsc=%s\n", p->ntscmode ? L"true" : L"false");
    cfgfile_write (f, L"genlock=%s\n", p->genlock ? L"true" : L"false");
    cfgfile_dwrite (f, L"show_leds=%s\n", (p->leds_on_screen & STATUSLINE_CHIPSET) ? L"true" : L"false");
    cfgfile_dwrite (f, L"keyboard_leds=numlock:%s,capslock:%s,scrolllock:%s\n",
	kbleds[p->keyboard_leds[0]], kbleds[p->keyboard_leds[1]], kbleds[p->keyboard_leds[2]]);
    if (p->chipset_mask & CSMASK_AGA)
	cfgfile_dwrite (f, L"chipset=aga\n");
    else if ((p->chipset_mask & CSMASK_ECS_AGNUS) && (p->chipset_mask & CSMASK_ECS_DENISE))
	cfgfile_dwrite (f, L"chipset=ecs\n");
    else if (p->chipset_mask & CSMASK_ECS_AGNUS)
	cfgfile_dwrite (f, L"chipset=ecs_agnus\n");
    else if (p->chipset_mask & CSMASK_ECS_DENISE)
	cfgfile_dwrite (f, L"chipset=ecs_denise\n");
    else
	cfgfile_dwrite (f, L"chipset=ocs\n");
    cfgfile_write (f, L"chipset_refreshrate=%d\n", p->chipset_refreshrate);
    cfgfile_write (f, L"collision_level=%s\n", collmode[p->collision_level]);

    cfgfile_write (f, L"chipset_compatible=%s\n", cscompa[p->cs_compatible]);
    cfgfile_dwrite (f, L"ciaatod=%s\n", ciaatodmode[p->cs_ciaatod]);
    cfgfile_dwrite (f, L"rtc=%s\n", rtctype[p->cs_rtc]);
    //cfgfile_dwrite (f, L"chipset_rtc_adjust=%d\n", p->cs_rtc_adjust);
    cfgfile_dwrite (f, L"ksmirror_e0=%s\n", p->cs_ksmirror_e0 ? L"true" : L"false");
    cfgfile_dwrite (f, L"ksmirror_a8=%s\n", p->cs_ksmirror_a8 ? L"true" : L"false");
    cfgfile_dwrite (f, L"cd32cd=%s\n", p->cs_cd32cd ? L"true" : L"false");
    cfgfile_dwrite (f, L"cd32c2p=%s\n", p->cs_cd32c2p ? L"true" : L"false");
    cfgfile_dwrite (f, L"cd32nvram=%s\n", p->cs_cd32nvram ? L"true" : L"false");
    cfgfile_dwrite (f, L"cdtvcd=%s\n", p->cs_cdtvcd ? L"true" : L"false");
    cfgfile_dwrite (f, L"cdtvram=%s\n", p->cs_cdtvram ? L"true" : L"false");
    cfgfile_dwrite (f, L"cdtvramcard=%d\n", p->cs_cdtvcard);
    cfgfile_dwrite (f, L"ide=%s\n", p->cs_ide == 1 ? L"a600/a1200" : (p->cs_ide == 2 ? L"a4000" : L"none"));
    cfgfile_dwrite (f, L"a1000ram=%s\n", p->cs_a1000ram ? L"true" : L"false");
    cfgfile_dwrite (f, L"fatgary=%d\n", p->cs_fatgaryrev);
    cfgfile_dwrite (f, L"ramsey=%d\n", p->cs_ramseyrev);
    cfgfile_dwrite (f, L"pcmcia=%s\n", p->cs_pcmcia ? L"true" : L"false");
    cfgfile_dwrite (f, L"scsi_cdtv=%s\n", p->cs_cdtvscsi ? L"true" : L"false");
    cfgfile_dwrite (f, L"scsi_a2091=%s\n", p->cs_a2091 ? L"true" : L"false");
    cfgfile_dwrite (f, L"scsi_a4091=%s\n", p->cs_a4091 ? L"true" : L"false");
    cfgfile_dwrite (f, L"scsi_a3000=%s\n", p->cs_mbdmac == 1 ? L"true" : L"false");
    cfgfile_dwrite (f, L"scsi_a4000t=%s\n", p->cs_mbdmac == 2 ? L"true" : L"false");
    cfgfile_dwrite (f, L"bogomem_fast=%s\n", p->cs_slowmemisfast ? L"true" : L"false");
    cfgfile_dwrite (f, L"resetwarning=%s\n", p->cs_resetwarning ? L"true" : L"false");
    cfgfile_dwrite (f, L"denise_noehb=%s\n", p->cs_denisenoehb ? L"true" : L"false");
    cfgfile_dwrite (f, L"agnus_bltbusybug=%s\n", p->cs_agnusbltbusybug ? L"true" : L"false");

    cfgfile_write (f, L"fastmem_size=%d\n", p->fastmem_size / 0x100000);
    cfgfile_write (f, L"a3000mem_size=%d\n", p->mbresmem_low_size / 0x100000);
    cfgfile_write (f, L"mbresmem_size=%d\n", p->mbresmem_high_size / 0x100000);
    cfgfile_write (f, L"z3mem_size=%d\n", p->z3fastmem_size / 0x100000);
    cfgfile_write (f, L"z3mem2_size=%d\n", p->z3fastmem2_size / 0x100000);
    cfgfile_write (f, L"z3mem_start=0x%x\n", p->z3fastmem_start);
    cfgfile_write (f, L"bogomem_size=%d\n", p->bogomem_size / 0x40000);
    cfgfile_write (f, L"gfxcard_size=%d\n", p->gfxmem_size / 0x100000);
    cfgfile_write (f, L"chipmem_size=%d\n", p->chipmem_size == 0x20000 ? -1 : (p->chipmem_size == 0x40000 ? 0 : p->chipmem_size / 0x80000));

    if (p->m68k_speed > 0)
	cfgfile_write (f, L"finegrain_cpu_speed=%d\n", p->m68k_speed);
    else
	cfgfile_write (f, L"cpu_speed=%s\n", p->m68k_speed == -1 ? L"max" : L"real");

    /* do not reorder start */
    write_compatibility_cpu(f, p);
    cfgfile_write (f, L"cpu_model=%d\n", p->cpu_model);
    if (p->fpu_model)
	cfgfile_write (f, L"fpu_model=%d\n", p->fpu_model);
    cfgfile_write (f, L"cpu_compatible=%s\n", p->cpu_compatible ? L"true" : L"false");
    cfgfile_write (f, L"cpu_24bit_addressing=%s\n", p->address_space_24 ? L"true" : L"false");
    /* do not reorder end */
    cfgfile_write (f, L"cpu_cycle_exact=%s\n", p->cpu_cycle_exact ? L"true" : L"false");
    cfgfile_write (f, L"blitter_cycle_exact=%s\n", p->blitter_cycle_exact ? L"true" : L"false");
    cfgfile_write (f, L"rtg_nocustom=%s\n", p->picasso96_nocustom ? L"true" : L"false");
    cfgfile_write (f, L"rtg_modes=0x%x\n", p->picasso96_modeflags);

    cfgfile_write (f, L"log_illegal_mem=%s\n", p->illegal_mem ? L"true" : L"false");
    if (p->catweasel >= 100)
	cfgfile_dwrite (f, L"catweasel=0x%x\n", p->catweasel);
    else
	cfgfile_dwrite (f, L"catweasel=%d\n", p->catweasel);

    cfgfile_write (f, L"kbd_lang=%s\n", (p->keyboard_lang == KBD_LANG_DE ? L"de"
					: p->keyboard_lang == KBD_LANG_DK ? L"dk"
					: p->keyboard_lang == KBD_LANG_ES ? L"es"
					: p->keyboard_lang == KBD_LANG_US ? L"us"
					: p->keyboard_lang == KBD_LANG_SE ? L"se"
					: p->keyboard_lang == KBD_LANG_FR ? L"fr"
					: p->keyboard_lang == KBD_LANG_IT ? L"it"
					: L"FOO"));

    cfgfile_dwrite (f, L"state_replay=%s\n", p->statecapture ? L"yes" : L"no");
    cfgfile_dwrite (f, L"state_replay_rate=%d\n", p->statecapturerate);
    cfgfile_dwrite (f, L"state_replay_buffer=%d\n", p->statecapturebuffersize);

#ifdef FILESYS
    write_filesys_config (p, UNEXPANDED, p->path_hardfile, f);
    if (p->filesys_no_uaefsdb)
	cfgfile_write (f, L"filesys_no_fsdb=%s\n", p->filesys_no_uaefsdb ? L"true" : L"false");
#endif
    write_inputdevice_config (p, f);

    /* Don't write gfxlib/gfx_test_speed options.  */
}

int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location)
{
    if (_tcscmp (option, name) != 0)
	return 0;
    if (strcasecmp (value, L"yes") == 0 || strcasecmp (value, L"y") == 0
	|| strcasecmp (value, L"true") == 0 || strcasecmp (value, L"t") == 0)
	*location = 1;
    else if (strcasecmp (value, L"no") == 0 || strcasecmp (value, L"n") == 0
	|| strcasecmp (value, L"false") == 0 || strcasecmp (value, L"f") == 0)
	*location = 0;
    else {
	write_log (L"Option `%s' requires a value of either `yes' or `no'.\n", option);
	return -1;
    }
    return 1;
}

int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, int scale)
{
    int base = 10;
    TCHAR *endptr;
    if (_tcscmp (option, name) != 0)
	return 0;
    /* I guess octal isn't popular enough to worry about here...  */
    if (value[0] == '0' && value[1] == 'x')
	value += 2, base = 16;
    *location = _tcstol (value, &endptr, base) * scale;

    if (*endptr != '\0' || *value == '\0') {
	write_log (L"Option `%s' requires a numeric argument.\n", option);
	return -1;
    }
    return 1;
}

int cfgfile_strval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, const TCHAR *table[], int more)
{
    int val;
    if (_tcscmp (option, name) != 0)
	return 0;
    val = match_string (table, value);
    if (val == -1) {
	if (more)
	    return 0;

	write_log (L"Unknown value for option `%s'.\n", option);
	return -1;
    }
    *location = val;
    return 1;
}

int cfgfile_string (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
    if (_tcscmp (option, name) != 0)
	return 0;
    _tcsncpy (location, value, maxsz - 1);
    location[maxsz - 1] = '\0';
    return 1;
}

static int getintval (TCHAR **p, int *result, int delim)
{
    TCHAR *value = *p;
    int base = 10;
    TCHAR *endptr;
    TCHAR *p2 = _tcschr (*p, delim);

    if (p2 == 0)
	return 0;

    *p2++ = '\0';

    if (value[0] == '0' && _totupper (value[1]) == 'X')
	value += 2, base = 16;
    *result = _tcstol (value, &endptr, base);
    *p = p2;

    if (*endptr != '\0' || *value == '\0')
	return 0;

    return 1;
}

static int getintval2 (TCHAR **p, int *result, int delim)
{
    TCHAR *value = *p;
    int base = 10;
    TCHAR *endptr;
    TCHAR *p2 = _tcschr (*p, delim);

    if (p2 == 0) {
	p2 = _tcschr (*p, 0);
	if (p2 == 0) {
	    *p = 0;
	    return 0;
	}
    }
    if (*p2 != 0)
	*p2++ = '\0';

    if (value[0] == '0' && _totupper (value[1]) == 'X')
	value += 2, base = 16;
    *result = _tcstol (value, &endptr, base);
    *p = p2;

    if (*endptr != '\0' || *value == '\0') {
	*p = 0;
	return 0;
    }

    return 1;
}

static void set_chipset_mask (struct uae_prefs *p, int val)
{
    p->chipset_mask = (val == 0 ? 0
		       : val == 1 ? CSMASK_ECS_AGNUS
		       : val == 2 ? CSMASK_ECS_DENISE
		       : val == 3 ? CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS
		       : CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS);
}

static int cfgfile_parse_host (struct uae_prefs *p, TCHAR *option, TCHAR *value)
{
    int i;
    TCHAR *section = 0;
    TCHAR *tmpp;
    TCHAR tmpbuf[CONFIG_BLEN];

    if (_tcsncmp (option, L"input.", 6) == 0) {
	read_inputdevice_config (p, option, value);
	return 1;
    }

    for (tmpp = option; *tmpp != '\0'; tmpp++)
	if (_istupper (*tmpp))
	    *tmpp = _totlower (*tmpp);
    tmpp = _tcschr (option, '.');
    if (tmpp) {
	section = option;
	option = tmpp + 1;
	*tmpp = '\0';
	if (_tcscmp (section, TARGET_NAME) == 0) {
	    /* We special case the various path options here.  */
	    if (cfgfile_string (option, value, L"rom_path", p->path_rom, sizeof p->path_rom / sizeof (TCHAR))
		|| cfgfile_string (option, value, L"floppy_path", p->path_floppy, sizeof p->path_floppy / sizeof (TCHAR))
		|| cfgfile_string (option, value, L"hardfile_path", p->path_hardfile, sizeof p->path_hardfile / sizeof (TCHAR)))
		return 1;
	    return target_parse_option (p, option, value);
	}
	return 0;
    }
    for (i = 0; i < MAX_SPARE_DRIVES; i++) {
	_stprintf (tmpbuf, L"diskimage%d", i);
	if (cfgfile_string (option, value, tmpbuf, p->dfxlist[i], sizeof p->dfxlist[i] / sizeof (TCHAR))) {
#if 0
	    if (i < 4 && !p->df[i][0])
		_tcscpy (p->df[i], p->dfxlist[i]);
#endif
	    return 1;
	}
    }

    if (cfgfile_intval (option, value, L"sound_frequency", &p->sound_freq, 1)) {
	/* backwards compatibility */
	p->sound_latency = 1000 * (p->sound_maxbsiz >> 1) / p->sound_freq;
	return 1;
    }

    if (cfgfile_intval (option, value, L"sound_latency", &p->sound_latency, 1)
	|| cfgfile_intval (option, value, L"sound_max_buff", &p->sound_maxbsiz, 1)
	|| cfgfile_intval (option, value, L"state_replay_rate", &p->statecapturerate, 1)
	|| cfgfile_intval (option, value, L"state_replay_buffer", &p->statecapturebuffersize, 1)
	|| cfgfile_intval (option, value, L"sound_frequency", &p->sound_freq, 1)
	|| cfgfile_intval (option, value, L"sound_volume", &p->sound_volume, 1)
	|| cfgfile_intval (option, value, L"sound_stereo_separation", &p->sound_stereo_separation, 1)
	|| cfgfile_intval (option, value, L"sound_stereo_mixing_delay", &p->sound_mixed_stereo_delay, 1)

	|| cfgfile_intval (option, value, L"gfx_display", &p->gfx_display, 1)
	|| cfgfile_intval (option, value, L"gfx_framerate", &p->gfx_framerate, 1)
	|| cfgfile_intval (option, value, L"gfx_width_windowed", &p->gfx_size_win.width, 1)
	|| cfgfile_intval (option, value, L"gfx_height_windowed", &p->gfx_size_win.height, 1)
	|| cfgfile_intval (option, value, L"gfx_top_windowed", &p->gfx_size_win.x, 1)
	|| cfgfile_intval (option, value, L"gfx_left_windowed", &p->gfx_size_win.y, 1)
	|| cfgfile_intval (option, value, L"gfx_width_fullscreen", &p->gfx_size_fs.width, 1)
	|| cfgfile_intval (option, value, L"gfx_height_fullscreen", &p->gfx_size_fs.height, 1)
	|| cfgfile_intval (option, value, L"gfx_refreshrate", &p->gfx_refreshrate, 1)
	|| cfgfile_intval (option, value, L"gfx_autoresolution", &p->gfx_autoresolution, 1)

	|| cfgfile_intval (option, value, L"gfx_center_horizontal_position", &p->gfx_xcenter_pos, 1)
	|| cfgfile_intval (option, value, L"gfx_center_vertical_position", &p->gfx_ycenter_pos, 1)
	|| cfgfile_intval (option, value, L"gfx_center_horizontal_size", &p->gfx_xcenter_size, 1)
	|| cfgfile_intval (option, value, L"gfx_center_vertical_size", &p->gfx_ycenter_size, 1)

#ifdef GFXFILTER
	|| cfgfile_intval (option, value, L"gfx_filter_vert_zoom", &p->gfx_filter_vert_zoom, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_horiz_zoom", &p->gfx_filter_horiz_zoom, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_vert_zoom_mult", &p->gfx_filter_vert_zoom_mult, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_horiz_zoom_mult", &p->gfx_filter_horiz_zoom_mult, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_vert_offset", &p->gfx_filter_vert_offset, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_horiz_offset", &p->gfx_filter_horiz_offset, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_scanlines", &p->gfx_filter_scanlines, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_scanlinelevel", &p->gfx_filter_scanlinelevel, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_scanlineratio", &p->gfx_filter_scanlineratio, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_luminance", &p->gfx_filter_luminance, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_contrast", &p->gfx_filter_contrast, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_saturation", &p->gfx_filter_saturation, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_gamma", &p->gfx_filter_gamma, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_blur", &p->gfx_filter_blur, 1)
	|| cfgfile_intval (option, value, L"gfx_filter_noise", &p->gfx_filter_noise, 1)
	|| cfgfile_yesno  (option, value, L"gfx_filter_keep_aspect", &p->gfx_filter_keep_aspect)
	|| cfgfile_intval (option, value, L"gfx_luminance", &p->gfx_luminance, 1)
	|| cfgfile_intval (option, value, L"gfx_contrast", &p->gfx_contrast, 1)
	|| cfgfile_intval (option, value, L"gfx_gamma", &p->gfx_gamma, 1)
#endif
	|| cfgfile_intval (option, value, L"floppy0sound", &p->dfxclick[0], 1)
	|| cfgfile_intval (option, value, L"floppy1sound", &p->dfxclick[1], 1)
	|| cfgfile_intval (option, value, L"floppy2sound", &p->dfxclick[2], 1)
	|| cfgfile_intval (option, value, L"floppy3sound", &p->dfxclick[3], 1)
	|| cfgfile_intval (option, value, L"floppy_volume", &p->dfxclickvolume, 1)
	|| cfgfile_intval (option, value, L"override_dga_address", &p->override_dga_address, 1))
	    return 1;

    if (cfgfile_string (option, value, L"floppy0soundext", p->dfxclickexternal[0], sizeof p->dfxclickexternal[0] / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"floppy1soundext", p->dfxclickexternal[1], sizeof p->dfxclickexternal[1] / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"floppy2soundext", p->dfxclickexternal[2], sizeof p->dfxclickexternal[2] / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"floppy3soundext", p->dfxclickexternal[3], sizeof p->dfxclickexternal[3] / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"gfx_display_name", p->gfx_display_name, sizeof p->gfx_display_name / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"config_info", p->info, sizeof p->info / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"config_description", p->description, sizeof p->description / sizeof (TCHAR)))
	    return 1;

    if (cfgfile_yesno (option, value, L"use_debugger", &p->start_debugger)
	|| cfgfile_yesno (option, value, L"sound_auto", &p->sound_auto)
	|| cfgfile_yesno (option, value, L"sound_stereo_swap_paula", &p->sound_stereo_swap_paula)
	|| cfgfile_yesno (option, value, L"sound_stereo_swap_ahi", &p->sound_stereo_swap_ahi)
	|| cfgfile_yesno (option, value, L"state_replay", &p->statecapture)
	|| cfgfile_yesno (option, value, L"avoid_cmov", &p->avoid_cmov)
	|| cfgfile_yesno (option, value, L"avoid_dga", &p->avoid_dga)
	|| cfgfile_yesno (option, value, L"avoid_vid", &p->avoid_vid)
	|| cfgfile_yesno (option, value, L"log_illegal_mem", &p->illegal_mem)
	|| cfgfile_yesno (option, value, L"filesys_no_fsdb", &p->filesys_no_uaefsdb)
	|| cfgfile_yesno (option, value, L"gfx_vsync", &p->gfx_avsync)
	|| cfgfile_yesno (option, value, L"gfx_vsync_picasso", &p->gfx_pvsync)
	|| cfgfile_yesno (option, value, L"gfx_blacker_than_black", &p->gfx_blackerthanblack)
	|| cfgfile_yesno (option, value, L"gfx_flickerfixer", &p->gfx_scandoubler)
	|| cfgfile_yesno (option, value, L"show_leds", &p->leds_on_screen)
	|| cfgfile_yesno (option, value, L"synchronize_clock", &p->tod_hack)
	|| cfgfile_yesno (option, value, L"magic_mouse", &p->input_magic_mouse)
	|| cfgfile_yesno (option, value, L"bsdsocket_emu", &p->socket_emu))
	    return 1;

    if (cfgfile_strval (option, value, L"sound_output", &p->produce_sound, soundmode1, 1)
	|| cfgfile_strval (option, value, L"sound_output", &p->produce_sound, soundmode2, 0)
	|| cfgfile_strval (option, value, L"sound_interpol", &p->sound_interpol, interpolmode, 0)
	|| cfgfile_strval (option, value, L"sound_filter", &p->sound_filter, soundfiltermode1, 0)
	|| cfgfile_strval (option, value, L"sound_filter_type", &p->sound_filter_type, soundfiltermode2, 0)
	|| cfgfile_strval (option, value, L"use_gui", &p->start_gui, guimode1, 1)
	|| cfgfile_strval (option, value, L"use_gui", &p->start_gui, guimode2, 1)
	|| cfgfile_strval (option, value, L"use_gui", &p->start_gui, guimode3, 0)
	|| cfgfile_strval (option, value, L"gfx_resolution", &p->gfx_resolution, lorestype1, 0)
	|| cfgfile_strval (option, value, L"gfx_lores", &p->gfx_resolution, lorestype2, 0)
	|| cfgfile_strval (option, value, L"gfx_lores_mode", &p->gfx_lores_mode, loresmode, 0)
	|| cfgfile_strval (option, value, L"gfx_fullscreen_amiga", &p->gfx_afullscreen, fullmodes, 0)
	|| cfgfile_strval (option, value, L"gfx_fullscreen_picasso", &p->gfx_pfullscreen, fullmodes, 0)
	|| cfgfile_strval (option, value, L"gfx_linemode", &p->gfx_linedbl, linemode1, 1)
	|| cfgfile_strval (option, value, L"gfx_linemode", &p->gfx_linedbl, linemode2, 0)
	|| cfgfile_strval (option, value, L"gfx_center_horizontal", &p->gfx_xcenter, centermode1, 1)
	|| cfgfile_strval (option, value, L"gfx_center_vertical", &p->gfx_ycenter, centermode1, 1)
	|| cfgfile_strval (option, value, L"gfx_center_horizontal", &p->gfx_xcenter, centermode2, 0)
	|| cfgfile_strval (option, value, L"gfx_center_vertical", &p->gfx_ycenter, centermode2, 0)
	|| cfgfile_strval (option, value, L"gfx_colour_mode", &p->color_mode, colormode1, 1)
	|| cfgfile_strval (option, value, L"gfx_colour_mode", &p->color_mode, colormode2, 0)
	|| cfgfile_strval (option, value, L"gfx_color_mode", &p->color_mode, colormode1, 1)
	|| cfgfile_strval (option, value, L"gfx_color_mode", &p->color_mode, colormode2, 0)
	|| cfgfile_strval (option, value, L"gfx_max_horizontal", &p->gfx_max_horizontal, maxhoriz, 0)
	|| cfgfile_strval (option, value, L"gfx_max_vertical", &p->gfx_max_vertical, maxvert, 0)
	|| cfgfile_strval (option, value, L"gfx_filter_autoscale", &p->gfx_filter_autoscale, autoscale, 0)
	|| cfgfile_strval (option, value, L"magic_mousecursor", &p->input_magic_mouse_cursor, magiccursors, 0)
	|| cfgfile_strval (option, value, L"absolute_mouse", &p->input_tablet, abspointers, 0))
	    return 1;


#ifdef GFXFILTER
    if (_tcscmp (option, L"gfx_filter") == 0) {
	int i = 0;
	TCHAR *s = _tcschr (value, ':');
	if (s)
	    *s++ = 0;
	p->gfx_filtershader[0] = 0;
	p->gfx_filter = 0;
	while(uaefilters[i].name) {
	    if (!_tcscmp (uaefilters[i].cfgname, value)) {
		p->gfx_filter = uaefilters[i].type;
		if (s)
		    _tcscpy (p->gfx_filtershader, s);
		break;
	    }
	    i++;
	}
	return 1;
    }
    if (_tcscmp (option, L"gfx_filter_mode") == 0) {
	p->gfx_filter_filtermode = 0;
	if (p->gfx_filter > 0) {
	    struct uae_filter *uf;
	    int i = 0;
	    while(uaefilters[i].name) {
		uf = &uaefilters[i];
		if (uf->type == p->gfx_filter) {
		    if (uf->x[0]) {
			cfgfile_strval (option, value, L"gfx_filter_mode", &p->gfx_filter_filtermode, filtermode1, 0);
		    } else {
			int mt[4], j;
			i = 0;
			if (uf->x[1])
			    mt[i++] = 1;
			if (uf->x[2])
			    mt[i++] = 2;
			if (uf->x[3])
			    mt[i++] = 3;
			if (uf->x[4])
			    mt[i++] = 4;
			cfgfile_strval (option, value, L"gfx_filter_mode", &i, filtermode2, 0);
			for (j = 0; j < i; j++) {
			    if (mt[j] == i)
				p->gfx_filter_filtermode = j;
			}
		    }
		    break;
		}
		i++;
	    }
	}
	return 1;
    }
    if (cfgfile_string (option, value, L"gfx_filter_aspect_ratio", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	int v1, v2;
	TCHAR *s;
	
	p->gfx_filter_aspect = -1;
	v1 = _tstol (tmpbuf);
	s = _tcschr (tmpbuf, ':');
	if (s) {
	    v2 = _tstol (s + 1);
	    if (v1 < 0 || v2 < 0)
		p->gfx_filter_aspect = -1;
	    else if (v1 == 0 || v2 == 0)
		p->gfx_filter_aspect = 0;
	    else
		p->gfx_filter_aspect = (v1 << 8) | v2;
	}
	return 1;
    }
#endif

    if (_tcscmp (option, L"gfx_width") == 0 || _tcscmp (option, L"gfx_height") == 0) {
	cfgfile_intval (option, value, L"gfx_width", &p->gfx_size_win.width, 1);
	cfgfile_intval (option, value, L"gfx_height", &p->gfx_size_win.height, 1);
	p->gfx_size_fs.width = p->gfx_size_win.width;
	p->gfx_size_fs.height = p->gfx_size_win.height;
	return 1;
    }

    if (_tcscmp (option, L"gfx_fullscreen_multi") == 0 || _tcscmp (option, L"gfx_windowed_multi") == 0) {
	TCHAR tmp[256], *tmpp, *tmpp2;
	struct wh *wh = p->gfx_size_win_xtra;
	if (_tcscmp (option, L"gfx_fullscreen_multi") == 0)
	    wh = p->gfx_size_fs_xtra;
	_stprintf (tmp, L",%s,", value);
	tmpp2 = tmp;
	for (i = 0; i < 4; i++) {
	    tmpp = _tcschr (tmpp2, ',');
	    tmpp++;
	    wh[i].width = _tstol (tmpp);
	    while (*tmpp != ',' && *tmpp != 'x')
		tmpp++;
	    wh[i].height = _tstol (tmpp + 1);
	    tmpp2 = tmpp;
	}
	return 1;
    }

    if (_tcscmp (option, L"joyportfriendlyname0") == 0 || _tcscmp (option, L"joyportfriendlyname1") == 0) {
	inputdevice_joyport_config (p, value, _tcscmp (option, L"joyportfriendlyname0") == 0 ? 0 : 1, 2);
	return 1;
    }
    if (_tcscmp (option, L"joyportname0") == 0 || _tcscmp (option, L"joyportname1") == 0) {
	inputdevice_joyport_config (p, value, _tcscmp (option, L"joyportname0") == 0 ? 0 : 1, 1);
	return 1;
    }

    if (_tcscmp (option, L"joyport0") == 0 || _tcscmp (option, L"joyport1") == 0) {
	inputdevice_joyport_config (p, value, _tcscmp (option, L"joyport0") == 0 ? 0 : 1, 0);
	return 1;
    }

    if (cfgfile_string (option, value, L"statefile", tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
	_tcscpy (savestate_fname, tmpbuf);
	if (zfile_exists (savestate_fname)) {
	    savestate_state = STATE_DORESTORE;
	} else {
	    int ok = 0;
	    if (savestate_fname[0]) {
		for (;;) {
		    TCHAR *p;
		    if (my_existsdir (savestate_fname)) {
		        ok = 1;
		        break;
		    }
		    p = _tcsrchr (savestate_fname, '\\');
		    if (!p)
		        p = _tcsrchr (savestate_fname, '/');
		    if (!p)
		        break;
		    *p = 0;
		}
	    }
	    if (!ok)
		savestate_fname[0] = 0;
	}
	return 1;
    }

    if (cfgfile_strval (option, value, L"sound_channels", &p->sound_stereo, stereomode, 1)) {
	if (p->sound_stereo == SND_NONE) { /* "mixed stereo" compatibility hack */
	    p->sound_stereo = SND_STEREO;
	    p->sound_mixed_stereo_delay = 5;
	    p->sound_stereo_separation = 7;
	}
	return 1;
    }

    if (_tcscmp (option, L"kbd_lang") == 0) {
	KbdLang l;
	if ((l = KBD_LANG_DE, strcasecmp (value, L"de") == 0)
	    || (l = KBD_LANG_DK, strcasecmp (value, L"dk") == 0)
	    || (l = KBD_LANG_SE, strcasecmp (value, L"se") == 0)
	    || (l = KBD_LANG_US, strcasecmp (value, L"us") == 0)
	    || (l = KBD_LANG_FR, strcasecmp (value, L"fr") == 0)
	    || (l = KBD_LANG_IT, strcasecmp (value, L"it") == 0)
	    || (l = KBD_LANG_ES, strcasecmp (value, L"es") == 0))
	    p->keyboard_lang = l;
	else
	    write_log (L"Unknown keyboard language\n");
	return 1;
    }

    if (cfgfile_string (option, value, L"config_version", tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
	TCHAR *tmpp2;
	tmpp = _tcschr (value, '.');
	if (tmpp) {
	    *tmpp++ = 0;
	    tmpp2 = tmpp;
	    p->config_version = _tstol (tmpbuf) << 16;
	    tmpp = _tcschr (tmpp, '.');
	    if (tmpp) {
		*tmpp++ = 0;
		p->config_version |= _tstol (tmpp2) << 8;
		p->config_version |= _tstol (tmpp);
	    }
	}
	return 1;
    }

    if (cfgfile_string (option, value, L"keyboard_leds", tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
	TCHAR *tmpp2 = tmpbuf;
	int i, num;
	p->keyboard_leds[0] = p->keyboard_leds[1] = p->keyboard_leds[2] = 0;
	p->keyboard_leds_in_use = 0;
	_tcscat (tmpbuf, L",");
	for (i = 0; i < 3; i++) {
	    tmpp = _tcschr (tmpp2, ':');
	    if (!tmpp)
		break;
	    *tmpp++= 0;
	    num = -1;
	    if (!strcasecmp (tmpp2, L"numlock"))
		num = 0;
	    if (!strcasecmp (tmpp2, L"capslock"))
		num = 1;
	    if (!strcasecmp (tmpp2, L"scrolllock"))
		num = 2;
	    tmpp2 = tmpp;
	    tmpp = _tcschr (tmpp2, ',');
	    if (!tmpp)
		break;
	    *tmpp++= 0;
	    if (num >= 0) {
		p->keyboard_leds[num] = match_string (kbleds, tmpp2);
		if (p->keyboard_leds[num]) p->keyboard_leds_in_use = 1;
	    }
	    tmpp2 = tmpp;
	}
	return 1;
    }

    return 0;
}

static void decode_rom_ident (TCHAR *romfile, int maxlen, TCHAR *ident)
{
    TCHAR *p;
    int ver, rev, subver, subrev, round, i;
    TCHAR model[64], *modelp;
    struct romlist **rl;
    TCHAR *romtxt;

    if (!ident[0])
	return;
    romtxt = malloc (10000 * sizeof (TCHAR));
    romtxt[0] = 0;
    for (round = 0; round < 2; round++) {
	ver = rev = subver = subrev = -1;
	modelp = NULL;
	memset (model, 0, sizeof model);
	p = ident;
	while (*p) {
	    TCHAR c = *p++;
	    int *pp1 = NULL, *pp2 = NULL;
	    if (_totupper(c) == 'V' && _istdigit(*p)) {
		pp1 = &ver;
		pp2 = &rev;
	    } else if (_totupper(c) == 'R' && _istdigit(*p)) {
		pp1 = &subver;
		pp2 = &subrev;
	    } else if (!_istdigit(c) && c != ' ') {
		_tcsncpy (model, p - 1, (sizeof model) - 1);
		p += _tcslen (model);
		modelp = model;
	    }
	    if (pp1) {
		*pp1 = _tstol(p);
		while (*p != 0 && *p != '.' && *p != ' ')
		p++;
		if (*p == '.') {
		p++;
		if (pp2)
			*pp2 = _tstol(p);
		}
	    }
	    if (*p == 0 || *p == ';') {
		rl = getromlistbyident (ver, rev, subver, subrev, modelp, round);
		if (rl) {
		    for (i = 0; rl[i]; i++) {
			if (round) {
			    TCHAR romname[MAX_DPATH];
			    getromname(rl[i]->rd, romname);
			    _tcscat (romtxt, romname);
			    _tcscat (romtxt, L"\n");
			} else {
			    _tcsncpy (romfile, rl[i]->path, maxlen);
			    goto end;
			}
		    }
		    xfree (rl);
		}
	    }
	}
    }
end:
    if (round && romtxt[0]) {
	notify_user_parms (NUMSG_ROMNEED, romtxt, romtxt);
    }
    xfree (romtxt);
}

static struct uaedev_config_info *getuci(struct uae_prefs *p)
{
    if (p->mountitems < MOUNT_CONFIG_SIZE)
	return &p->mountconfig[p->mountitems++];
    return NULL;
}

struct uaedev_config_info *add_filesys_config (struct uae_prefs *p, int index,
			TCHAR *devname, TCHAR *volname, TCHAR *rootdir, int readonly,
			int secspertrack, int surfaces, int reserved,
			int blocksize, int bootpri,
			TCHAR *filesysdir, int hdc, int flags) {
    struct uaedev_config_info *uci;
    int i;
    TCHAR *s;

    if (index < 0) {
	uci = getuci(p);
	uci->configoffset = -1;
    } else {
	uci = &p->mountconfig[index];
    }
    if (!uci)
	return 0;
    uci->ishdf = volname == NULL ? 1 : 0;
    _tcscpy (uci->devname, devname ? devname : L"");
    _tcscpy (uci->volname, volname ? volname : L"");
    _tcscpy (uci->rootdir, rootdir ? rootdir : L"");
    uci->readonly = readonly;
    uci->sectors = secspertrack;
    uci->surfaces = surfaces;
    uci->reserved = reserved;
    uci->blocksize = blocksize;
    uci->bootpri = bootpri;
    uci->donotmount = 0;
    uci->autoboot = 0;
    if (bootpri < -128)
	uci->donotmount = 1;
    else if (bootpri >= -127)
	uci->autoboot = 1;
    uci->controller = hdc;
    _tcscpy (uci->filesys, filesysdir ? filesysdir : L"");
    if (!uci->devname[0]) {
	TCHAR base[32];
	TCHAR base2[32];
	int num = 0;
	if (uci->rootdir[0] == 0 && !uci->ishdf)
	    _tcscpy (base, L"RDH");
	else
	    _tcscpy (base, L"DH");
	_tcscpy (base2, base);
	for (i = 0; i < p->mountitems; i++) {
	    _stprintf (base2, L"%s%d", base, num);
	    if (!_tcscmp(base2, p->mountconfig[i].devname)) {
		num++;
		i = -1;
		continue;
	    }
	}
	_tcscpy (uci->devname, base2);
    }
    s = filesys_createvolname (volname, rootdir, L"Harddrive");
    _tcscpy (uci->volname, s);
    xfree (s);
    return uci;
}

static void parse_addmem (struct uae_prefs *p, TCHAR *buf, int num)
{
    uae_u32 size = 0, addr = 0;

    if (!getintval2 (&buf, &addr, ','))
	return;
    if (!getintval2 (&buf, &size, 0))
	return;
    if (addr & 0xffff)
	return;
    if ((size & 0xffff) || (size & 0xffff0000) == 0)
	return;
    p->custom_memory_addrs[num] = addr;
    p->custom_memory_sizes[num] = size;
}

static int cfgfile_parse_hardware (struct uae_prefs *p, TCHAR *option, TCHAR *value)
{
    int tmpval, dummyint, i;
    TCHAR *section = 0;
    TCHAR tmpbuf[CONFIG_BLEN];

    if (cfgfile_yesno (option, value, L"cpu_cycle_exact", &p->cpu_cycle_exact)
	|| cfgfile_yesno (option, value, L"blitter_cycle_exact", &p->blitter_cycle_exact)) {
	if (p->cpu_model >= 68020 && p->cachesize > 0)
	    p->cpu_cycle_exact = p->blitter_cycle_exact = 0;
	/* we don't want cycle-exact in 68020/40+JIT modes */
	return 1;
    }

    if (cfgfile_yesno (option, value, L"scsi_a3000", &dummyint)) {
	if (dummyint)
	    p->cs_mbdmac = 1;
	return 1;
    }
    if (cfgfile_yesno (option, value, L"scsi_a4000t", &dummyint)) {
	if (dummyint)
	    p->cs_mbdmac = 2;
	return 1;
    }
    if (cfgfile_yesno (option, value, L"immediate_blits", &p->immediate_blits)
	|| cfgfile_yesno (option, value, L"cd32cd", &p->cs_cd32cd)
	|| cfgfile_yesno (option, value, L"cd32c2p", &p->cs_cd32c2p)
	|| cfgfile_yesno (option, value, L"cd32nvram", &p->cs_cd32nvram)
	|| cfgfile_yesno (option, value, L"cdtvcd", &p->cs_cdtvcd)
	|| cfgfile_yesno (option, value, L"cdtvram", &p->cs_cdtvram)
	|| cfgfile_yesno (option, value, L"a1000ram", &p->cs_a1000ram)
	|| cfgfile_yesno (option, value, L"pcmcia", &p->cs_pcmcia)
	|| cfgfile_yesno (option, value, L"scsi_cdtv", &p->cs_cdtvscsi)
	|| cfgfile_yesno (option, value, L"scsi_a4091", &p->cs_a4091)
	|| cfgfile_yesno (option, value, L"scsi_a2091", &p->cs_a2091)
	|| cfgfile_yesno (option, value, L"cia_overlay", &p->cs_ciaoverlay)
	|| cfgfile_yesno (option, value, L"bogomem_fast", &p->cs_slowmemisfast)
	|| cfgfile_yesno (option, value, L"ksmirror_e0", &p->cs_ksmirror_e0)
	|| cfgfile_yesno (option, value, L"ksmirror_a8", &p->cs_ksmirror_a8)
	|| cfgfile_yesno (option, value, L"resetwarning", &p->cs_resetwarning)
	|| cfgfile_yesno (option, value, L"denise_noehb", &p->cs_denisenoehb)
	|| cfgfile_yesno (option, value, L"agnus_bltbusybug", &p->cs_agnusbltbusybug)

	|| cfgfile_yesno (option, value, L"kickshifter", &p->kickshifter)
	|| cfgfile_yesno (option, value, L"ntsc", &p->ntscmode)
	|| cfgfile_yesno (option, value, L"sana2", &p->sana2)
	|| cfgfile_yesno (option, value, L"genlock", &p->genlock)
	|| cfgfile_yesno (option, value, L"cpu_compatible", &p->cpu_compatible)
	|| cfgfile_yesno (option, value, L"cpu_24bit_addressing", &p->address_space_24)
	|| cfgfile_yesno (option, value, L"parallel_on_demand", &p->parallel_demand)
	|| cfgfile_yesno (option, value, L"parallel_postscript_emulation", &p->parallel_postscript_emulation)
	|| cfgfile_yesno (option, value, L"parallel_postscript_detection", &p->parallel_postscript_detection)
	|| cfgfile_yesno (option, value, L"serial_on_demand", &p->serial_demand)
	|| cfgfile_yesno (option, value, L"serial_hardware_ctsrts", &p->serial_hwctsrts)
	|| cfgfile_yesno (option, value, L"serial_direct", &p->serial_direct)
	|| cfgfile_yesno (option, value, L"comp_nf", &p->compnf)
	|| cfgfile_yesno (option, value, L"comp_constjump", &p->comp_constjump)
	|| cfgfile_yesno (option, value, L"comp_oldsegv", &p->comp_oldsegv)
	|| cfgfile_yesno (option, value, L"compforcesettings", &dummyint)
	|| cfgfile_yesno (option, value, L"compfpu", &p->compfpu)
	|| cfgfile_yesno (option, value, L"fpu_strict", &p->fpu_strict)
	|| cfgfile_yesno (option, value, L"comp_midopt", &p->comp_midopt)
	|| cfgfile_yesno (option, value, L"comp_lowopt", &p->comp_lowopt)
	|| cfgfile_yesno (option, value, L"rtg_nocustom", &p->picasso96_nocustom)
	|| cfgfile_yesno (option, value, L"uaeserial", &p->uaeserial))
	return 1;

    if (cfgfile_intval (option, value, L"cachesize", &p->cachesize, 1)
	|| cfgfile_intval (option, value, L"cpu060_revision", &p->cpu060_revision, 1)
	|| cfgfile_intval (option, value, L"fpu_revision", &p->fpu_revision, 1)
	|| cfgfile_intval (option, value, L"cdtvramcard", &p->cs_cdtvcard, 1)
	|| cfgfile_intval (option, value, L"fatgary", &p->cs_fatgaryrev, 1)
	|| cfgfile_intval (option, value, L"ramsey", &p->cs_ramseyrev, 1)
	|| cfgfile_intval (option, value, L"chipset_refreshrate", &p->chipset_refreshrate, 1)
	|| cfgfile_intval (option, value, L"fastmem_size", &p->fastmem_size, 0x100000)
	|| cfgfile_intval (option, value, L"a3000mem_size", &p->mbresmem_low_size, 0x100000)
	|| cfgfile_intval (option, value, L"mbresmem_size", &p->mbresmem_high_size, 0x100000)
	|| cfgfile_intval (option, value, L"z3mem_size", &p->z3fastmem_size, 0x100000)
	|| cfgfile_intval (option, value, L"z3mem2_size", &p->z3fastmem2_size, 0x100000)
	|| cfgfile_intval (option, value, L"z3mem_start", &p->z3fastmem_start, 1)
	|| cfgfile_intval (option, value, L"bogomem_size", &p->bogomem_size, 0x40000)
	|| cfgfile_intval (option, value, L"gfxcard_size", &p->gfxmem_size, 0x100000)
	|| cfgfile_intval (option, value, L"rtg_modes", &p->picasso96_modeflags, 1)
	|| cfgfile_intval (option, value, L"floppy_speed", &p->floppy_speed, 1)
	|| cfgfile_intval (option, value, L"floppy_write_length", &p->floppy_write_length, 1)
	|| cfgfile_intval (option, value, L"nr_floppies", &p->nr_floppies, 1)
	|| cfgfile_intval (option, value, L"floppy0type", &p->dfxtype[0], 1)
	|| cfgfile_intval (option, value, L"floppy1type", &p->dfxtype[1], 1)
	|| cfgfile_intval (option, value, L"floppy2type", &p->dfxtype[2], 1)
	|| cfgfile_intval (option, value, L"floppy3type", &p->dfxtype[3], 1)
	|| cfgfile_intval (option, value, L"maprom", &p->maprom, 1)
	|| cfgfile_intval (option, value, L"parallel_autoflush", &p->parallel_autoflush_time, 1)
	|| cfgfile_intval (option, value, L"uae_hide", &p->uae_hide, 1)
	|| cfgfile_intval (option, value, L"catweasel", &p->catweasel, 1))
	return 1;

    if (cfgfile_strval (option, value, L"comp_trustbyte", &p->comptrustbyte, compmode, 0)
	|| cfgfile_strval (option, value, L"chipset_compatible", &p->cs_compatible, cscompa, 0)
	|| cfgfile_strval (option, value, L"rtc", &p->cs_rtc, rtctype, 0)
	|| cfgfile_strval (option, value, L"ksmirror", &p->cs_ksmirror_e0, ksmirrortype, 0)
	|| cfgfile_strval (option, value, L"ciaatod", &p->cs_ciaatod, ciaatodmode, 0)
	|| cfgfile_strval (option, value, L"ide", &p->cs_ide, idemode, 0)
	|| cfgfile_strval (option, value, L"scsi", &p->scsi, scsimode, 0)
	|| cfgfile_strval (option, value, L"comp_trustword", &p->comptrustword, compmode, 0)
	|| cfgfile_strval (option, value, L"comp_trustlong", &p->comptrustlong, compmode, 0)
	|| cfgfile_strval (option, value, L"comp_trustnaddr", &p->comptrustnaddr, compmode, 0)
	|| cfgfile_strval (option, value, L"collision_level", &p->collision_level, collmode, 0)
	|| cfgfile_strval (option, value, L"comp_flushmode", &p->comp_hardflush, flushmode, 0))
	return 1;

    if (cfgfile_string (option, value, L"kickstart_rom_file", p->romfile, sizeof p->romfile / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"kickstart_ext_rom_file", p->romextfile, sizeof p->romextfile / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"amax_rom_file", p->amaxromfile, sizeof p->amaxromfile / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"flash_file", p->flashfile, sizeof p->flashfile / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"cart_file", p->cartfile, sizeof p->cartfile / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"pci_devices", p->pci_devices, sizeof p->pci_devices / sizeof (TCHAR))
	|| cfgfile_string (option, value, L"ghostscript_parameters", p->ghostscript_parameters, sizeof p->ghostscript_parameters / sizeof (TCHAR)))
	return 1;

    if (cfgfile_strval (option, value, L"cart_internal", &p->cart_internal, cartsmode, 0)) {
	if (p->cart_internal) {
	    struct romdata *rd = getromdatabyid (63);
	    if (rd)
		_stprintf (p->cartfile, L":%s", rd->configname);
	}
	return 1;
    }
    if (cfgfile_string (option, value, L"kickstart_rom", p->romident, sizeof p->romident / sizeof (TCHAR))) {
	decode_rom_ident (p->romfile, sizeof p->romfile / sizeof (TCHAR), p->romident);
	return 1;
    }
    if (cfgfile_string (option, value, L"kickstart_ext_rom", p->romextident, sizeof p->romextident / sizeof (TCHAR))) {
	decode_rom_ident (p->romextfile, sizeof p->romextfile / sizeof (TCHAR), p->romextident);
	return 1;
    }
    if (cfgfile_string (option, value, L"cart", p->cartident, sizeof p->cartident / sizeof (TCHAR))) {
	decode_rom_ident (p->cartfile, sizeof p->cartfile / sizeof (TCHAR), p->cartident);
	return 1;
    }

    for (i = 0; i < 4; i++) {
	_stprintf (tmpbuf, L"floppy%d", i);
	if (cfgfile_string (option, value, tmpbuf, p->df[i], sizeof p->df[i] / sizeof (TCHAR)))
	    return 1;
    }

    if (cfgfile_intval (option, value, L"chipmem_size", &dummyint, 1)) {
	if (dummyint < 0)
	    p->chipmem_size = 0x20000; /* 128k, prototype support */
	else if (dummyint == 0)
	    p->chipmem_size = 0x40000; /* 256k */
	else
	    p->chipmem_size = dummyint * 0x80000;
	return 1;
    }

    if (cfgfile_string (option, value, L"addmem1", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	parse_addmem (p, tmpbuf, 0);
	return 1;
    }
    if (cfgfile_string (option, value, L"addmem2", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	parse_addmem (p, tmpbuf, 1);
	return 1;
    }

    if (cfgfile_strval (option, value, L"chipset", &tmpval, csmode, 0)) {
	set_chipset_mask (p, tmpval);
	return 1;
    }

    if (cfgfile_string (option, value, L"fpu_model", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	p->fpu_model = _tstol(tmpbuf);
	return 1;
    }

    if (cfgfile_string (option, value, L"cpu_model", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	p->cpu_model = _tstol(tmpbuf);
	p->fpu_model = 0;
	return 1;
    }

    /* old-style CPU configuration */
    if (cfgfile_string (option, value, L"cpu_type", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	p->fpu_model = 0;
	p->address_space_24 = 0;
	p->cpu_model = 680000;
	if (!_tcscmp (tmpbuf, L"68000")) {
	    p->cpu_model = 68000;
	} else if (!_tcscmp (tmpbuf, L"68010")) {
	    p->cpu_model = 68010;
	} else if (!_tcscmp (tmpbuf, L"68ec020")) {
	    p->cpu_model = 68020;
	    p->address_space_24 = 1;
	} else if (!_tcscmp (tmpbuf, L"68020")) {
	    p->cpu_model = 68020;
	} else if (!_tcscmp (tmpbuf, L"68ec020/68881")) {
	    p->cpu_model = 68020;
	    p->fpu_model = 68881;
	    p->address_space_24 = 1;
	} else if (!_tcscmp (tmpbuf, L"68020/68881")) {
	    p->cpu_model = 68020;
	    p->fpu_model = 68881;
	} else if (!_tcscmp (tmpbuf, L"68040")) {
	    p->cpu_model = 68040;
	    p->fpu_model = 68040;
	} else if (!_tcscmp (tmpbuf, L"68060")) {
	    p->cpu_model = 68060;
	    p->fpu_model = 68060;
	}
	return 1;
    }

    if (p->config_version < (21 << 16)) {
	if (cfgfile_strval (option, value, L"cpu_speed", &p->m68k_speed, speedmode, 1)
	    /* Broken earlier versions used to write this out as a string.  */
	    || cfgfile_strval (option, value, L"finegraincpu_speed", &p->m68k_speed, speedmode, 1))
	{
	    p->m68k_speed--;
	    return 1;
	}
    }

    if (cfgfile_intval (option, value, L"cpu_speed", &p->m68k_speed, 1)) {
	p->m68k_speed *= CYCLE_UNIT;
	return 1;
    }

    if (cfgfile_intval (option, value, L"finegrain_cpu_speed", &p->m68k_speed, 1)) {
	if (OFFICIAL_CYCLE_UNIT > CYCLE_UNIT) {
	    int factor = OFFICIAL_CYCLE_UNIT / CYCLE_UNIT;
	    p->m68k_speed = (p->m68k_speed + factor - 1) / factor;
	}
	if (strcasecmp (value, L"max") == 0)
	    p->m68k_speed = -1;
	return 1;
    }

    for (i = 0; i < MAX_FILESYSTEM_UNITS; i++) {
	TCHAR tmp[100];
	_stprintf (tmp, L"uaehf%d", i);
	if (_tcscmp (option, tmp) == 0)
	    return 1;
    }
 
    if (_tcscmp (option, L"filesystem") == 0
	|| _tcscmp (option, L"hardfile") == 0)
    {
	int secs, heads, reserved, bs, ro;
	TCHAR *aname, *root;
	TCHAR *tmpp = _tcschr (value, ',');
	TCHAR *str;

	if (config_newfilesystem)
	    return 1;

	if (tmpp == 0)
	    goto invalid_fs;

	*tmpp++ = '\0';
	if (_tcscmp (value, L"1") == 0 || strcasecmp (value, L"ro") == 0
	    || strcasecmp (value, L"readonly") == 0
	    || strcasecmp (value, L"read-only") == 0)
	    ro = 1;
	else if (_tcscmp (value, L"0") == 0 || strcasecmp (value, L"rw") == 0
		 || strcasecmp (value, L"readwrite") == 0
		 || strcasecmp (value, L"read-write") == 0)
	    ro = 0;
	else
	    goto invalid_fs;
	secs = 0; heads = 0; reserved = 0; bs = 0;

	value = tmpp;
	if (_tcscmp (option, L"filesystem") == 0) {
	    tmpp = _tcschr (value, ':');
	    if (tmpp == 0)
		goto invalid_fs;
	    *tmpp++ = '\0';
	    aname = value;
	    root = tmpp;
	} else {
	    if (! getintval (&value, &secs, ',')
		|| ! getintval (&value, &heads, ',')
		|| ! getintval (&value, &reserved, ',')
		|| ! getintval (&value, &bs, ','))
		goto invalid_fs;
	    root = value;
	    aname = 0;
	}
	str = cfgfile_subst_path (UNEXPANDED, p->path_hardfile, root);
#ifdef FILESYS
	add_filesys_config (p, -1, NULL, aname, str, ro, secs, heads, reserved, bs, 0, NULL, 0, 0);
#endif
	free (str);
	return 1;

    }

    if (_tcscmp (option, L"filesystem2") == 0
	|| _tcscmp (option, L"hardfile2") == 0)
    {
	int secs, heads, reserved, bs, ro, bp, hdcv;
	TCHAR *dname = NULL, *aname = L"", *root = NULL, *fs = NULL, *hdc;
	TCHAR *tmpp = _tcschr (value, ',');
	TCHAR *str = NULL;

	config_newfilesystem = 1;
	if (tmpp == 0)
	    goto invalid_fs;

	*tmpp++ = '\0';
	if (strcasecmp (value, L"ro") == 0)
	    ro = 1;
	else if (strcasecmp (value, L"rw") == 0)
	    ro = 0;
	else
	    goto invalid_fs;
	secs = 0; heads = 0; reserved = 0; bs = 0; bp = 0;
	fs = 0; hdc = 0; hdcv = 0;

	value = tmpp;
	if (_tcscmp (option, L"filesystem2") == 0) {
	    tmpp = _tcschr (value, ':');
	    if (tmpp == 0)
		goto empty_fs;
	    *tmpp++ = 0;
	    dname = value;
	    aname = tmpp;
	    tmpp = _tcschr (tmpp, ':');
	    if (tmpp == 0)
		goto empty_fs;
	    *tmpp++ = 0;
	    root = tmpp;
	    tmpp = _tcschr (tmpp, ',');
	    if (tmpp == 0)
		goto empty_fs;
	    *tmpp++ = 0;
	    if (! getintval (&tmpp, &bp, 0))
		goto empty_fs;
	} else {
	    tmpp = _tcschr (value, ':');
	    if (tmpp == 0)
		goto invalid_fs;
	    *tmpp++ = '\0';
	    dname = value;
	    root = tmpp;
	    tmpp = _tcschr (tmpp, ',');
	    if (tmpp == 0)
		goto invalid_fs;
	    *tmpp++ = 0;
	    aname = 0;
	    if (! getintval (&tmpp, &secs, ',')
		|| ! getintval (&tmpp, &heads, ',')
		|| ! getintval (&tmpp, &reserved, ',')
		|| ! getintval (&tmpp, &bs, ','))
		goto invalid_fs;
	    if (getintval2 (&tmpp, &bp, ',')) {
		fs = tmpp;
		tmpp = _tcschr (tmpp, ',');
		if (tmpp != 0) {
		    *tmpp++ = 0;
		    hdc = tmpp;
		    if(_tcslen (hdc) >= 4 && !_tcsncmp (hdc, L"ide", 3)) {
			hdcv = hdc[3] - '0' + HD_CONTROLLER_IDE0;
			if (hdcv < HD_CONTROLLER_IDE0 || hdcv > HD_CONTROLLER_IDE3)
			    hdcv = 0;
		    }
		    if(_tcslen (hdc) >= 5 && !_tcsncmp (hdc, L"scsi", 4)) {
			hdcv = hdc[4] - '0' + HD_CONTROLLER_SCSI0;
			if (hdcv < HD_CONTROLLER_SCSI0 || hdcv > HD_CONTROLLER_SCSI6)
			    hdcv = 0;
		    }
		    if (_tcslen (hdc) >= 6 && !_tcsncmp (hdc, L"scsram", 6))
			hdcv = HD_CONTROLLER_PCMCIA_SRAM;
		}
	    }
	}
      empty_fs:
	if (root)
	    str = cfgfile_subst_path (UNEXPANDED, p->path_hardfile, root);
#ifdef FILESYS
	add_filesys_config (p, -1, dname, aname, str, ro, secs, heads, reserved, bs, bp, fs, hdcv, 0);
#endif
	free (str);
	return 1;

      invalid_fs:
	write_log (L"Invalid filesystem/hardfile specification.\n");
	return 1;
    }

    return 0;
}

int cfgfile_parse_option (struct uae_prefs *p, TCHAR *option, TCHAR *value, int type)
{
    if (!_tcscmp (option, L"config_hardware"))
	return 1;
    if (!_tcscmp (option, L"config_host"))
	return 1;
    if (cfgfile_string (option, value, L"config_hardware_path", p->config_hardware_path, sizeof p->config_hardware_path / sizeof (TCHAR)))
	return 1;
    if (cfgfile_string (option, value, L"config_host_path", p->config_host_path, sizeof p->config_host_path / sizeof (TCHAR)))
	return 1;
    if (type == 0 || (type & CONFIG_TYPE_HARDWARE)) {
	if (cfgfile_parse_hardware (p, option, value))
	    return 1;
    }
    if (type == 0 || (type & CONFIG_TYPE_HOST)) {
	if (cfgfile_parse_host (p, option, value))
	    return 1;
    }
    if (type > 0)
	return 1;
    return 0;
}

static int cfgfile_separate_line (TCHAR *line, TCHAR *line1b, TCHAR *line2b)
{
    TCHAR *line1, *line2;
    int i;

    line1 = line;
    line2 = _tcschr (line, '=');
    if (! line2) {
	write_log (L"CFGFILE: line was incomplete with only %s\n", line1);
	return 0;
    }
    *line2++ = '\0';
    _tcscpy (line1b, line1);
    _tcscpy (line2b, line2);

    /* Get rid of whitespace.  */
    i = _tcslen (line2);
    while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
		     || line2[i - 1] == '\r' || line2[i - 1] == '\n'))
	line2[--i] = '\0';
    line2 += _tcsspn (line2, L"\t \r\n");
    _tcscpy (line2b, line2);
    i = _tcslen (line);
    while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
		     || line[i - 1] == '\r' || line[i - 1] == '\n'))
	line[--i] = '\0';
    line += _tcsspn (line, L"\t \r\n");
    _tcscpy (line1b, line);
    return 1;
}

static int isobsolete (TCHAR *s)
{
    int i = 0;
    while (obsolete[i]) {
	if (!strcasecmp (s, obsolete[i])) {
	    write_log (L"obsolete config entry '%s'\n", s);
	    return 1;
	}
	i++;
    }
    if (_tcslen (s) >= 10 && !_tcsncmp (s, L"gfx_opengl", 10)) {
	write_log (L"obsolete config entry '%s\n", s);
	return 1;
    }
    if (_tcslen (s) >= 6 && !_tcsncmp (s, L"gfx_3d", 6)) {
	write_log (L"obsolete config entry '%s\n", s);
	return 1;
    }
    return 0;
}

static void cfgfile_parse_separated_line (struct uae_prefs *p, TCHAR *line1b, TCHAR *line2b, int type)
{
    TCHAR line3b[CONFIG_BLEN], line4b[CONFIG_BLEN];
    struct strlist *sl;
    int ret;

    _tcscpy (line3b, line1b);
    _tcscpy (line4b, line2b);
    ret = cfgfile_parse_option (p, line1b, line2b, type);
    if (!isobsolete (line3b)) {
	for (sl = p->all_lines; sl; sl = sl->next) {
	    if (sl->option && !strcasecmp (line1b, sl->option)) break;
	}
	if (!sl) {
	    struct strlist *u = xcalloc (sizeof (struct strlist), 1);
	    u->option = my_strdup (line3b);
	    u->value = my_strdup (line4b);
	    u->next = p->all_lines;
	    p->all_lines = u;
	    if (!ret) {
		u->unknown = 1;
		write_log (L"unknown config entry: '%s=%s'\n", u->option, u->value);
	    }
	}
    }
}

void cfgfile_parse_line (struct uae_prefs *p, TCHAR *line, int type)
{
    TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

    if (!cfgfile_separate_line (line, line1b, line2b))
	return;
    cfgfile_parse_separated_line (p, line1b, line2b, type);
}

static void subst (TCHAR *p, TCHAR *f, int n)
{
    TCHAR *str = cfgfile_subst_path (UNEXPANDED, p, f);
    _tcsncpy (f, str, n - 1);
    f[n - 1] = '\0';
    free (str);
}

static TCHAR *cfg_fgets (TCHAR *line, int max, struct zfile *fh)
{
#ifdef SINGLEFILE
    extern TCHAR singlefile_config[];
    static TCHAR *sfile_ptr;
    TCHAR *p;
#endif

    if (fh)
	return zfile_fgets (line, max, fh);
#ifdef SINGLEFILE
    if (sfile_ptr == 0) {
	sfile_ptr = singlefile_config;
	if (*sfile_ptr) {
	    write_log (L"singlefile config found\n");
	    while (*sfile_ptr++);
	}
    }
    if (*sfile_ptr == 0) {
	sfile_ptr = singlefile_config;
	return 0;
    }
    p = sfile_ptr;
    while (*p != 13 && *p != 10 && *p != 0) p++;
    memset (line, 0, max);
    memcpy (line, sfile_ptr, (p - sfile_ptr) * sizeof (TCHAR));
    sfile_ptr = p + 1;
    if (*sfile_ptr == 13)
	sfile_ptr++;
    if (*sfile_ptr == 10)
	sfile_ptr++;
    return line;
#endif
    return 0;
}

static int cfgfile_load_2 (struct uae_prefs *p, const TCHAR *filename, int real, int *type)
{
    int i;
    struct zfile *fh;
    TCHAR line[CONFIG_BLEN], line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];
    struct strlist *sl;
    int type1 = 0, type2 = 0, askedtype = 0;

    if (type) {
	askedtype = *type;
	*type = 0;
    }
    if (real) {
	p->config_version = 0;
	config_newfilesystem = 0;
	reset_inputdevice_config (p);
    }

    fh = zfile_fopen (filename, L"r");
#ifndef	SINGLEFILE
    if (! fh)
	return 0;
#endif

    while (cfg_fgets (line, sizeof (line) / sizeof (TCHAR), fh) != 0) {
	trimws (line);
	if (_tcslen (line) > 0) {
	    if (line[0] == '#' || line[0] == ';')
		continue;
	    if (!cfgfile_separate_line (line, line1b, line2b))
		continue;
	    type1 = type2 = 0;
	    if (cfgfile_yesno (line1b, line2b, L"config_hardware", &type1) ||
		cfgfile_yesno (line1b, line2b, L"config_host", &type2)) {
		if (type1 && type)
		    *type |= CONFIG_TYPE_HARDWARE;
		if (type2 && type)
		    *type |= CONFIG_TYPE_HOST;
		continue;
	    }
	    if (real) {
		cfgfile_parse_separated_line (p, line1b, line2b, askedtype);
	    } else {
		cfgfile_string (line1b, line2b, L"config_description", p->description, sizeof p->description / sizeof (TCHAR));
		cfgfile_string (line1b, line2b, L"config_hardware_path", p->config_hardware_path, sizeof p->config_hardware_path / sizeof (TCHAR));
		cfgfile_string (line1b, line2b, L"config_host_path", p->config_host_path, sizeof p->config_host_path / sizeof (TCHAR));
	    }
	}
    }

    if (type && *type == 0)
	*type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
    zfile_fclose (fh);

    if (!real)
	return 1;

    for (sl = temp_lines; sl; sl = sl->next) {
	_stprintf (line, L"%s=%s", sl->option, sl->value);
	cfgfile_parse_line (p, line, 0);
    }

    for (i = 0; i < 4; i++)
	subst (p->path_floppy, p->df[i], sizeof p->df[i]);
    subst (p->path_rom, p->romfile, sizeof p->romfile / sizeof (TCHAR));
    subst (p->path_rom, p->romextfile, sizeof p->romextfile / sizeof (TCHAR));

    return 1;
}

int cfgfile_load (struct uae_prefs *p, const TCHAR *filename, int *type, int ignorelink)
{
    int v;
    TCHAR tmp[MAX_DPATH];
    int type2;
    static int recursive;

    if (recursive > 1)
	return 0;
    recursive++;
    write_log (L"load config '%s':%d\n", filename, type ? *type : -1);
    v = cfgfile_load_2 (p, filename, 1, type);
    if (!v) {
	write_log (L"load failed\n");
	goto end;
    }
    if (!ignorelink) {
	if (p->config_hardware_path[0]) {
	    fetch_configurationpath (tmp, sizeof (tmp) / sizeof (TCHAR));
	    _tcsncat (tmp, p->config_hardware_path, sizeof (tmp) / sizeof (TCHAR));
	    type2 = CONFIG_TYPE_HARDWARE;
	    cfgfile_load (p, tmp, &type2, 1);
	}
	if (p->config_host_path[0]) {
	    fetch_configurationpath (tmp, sizeof (tmp) / sizeof (TCHAR));
	    _tcsncat (tmp, p->config_host_path, sizeof (tmp) / sizeof (TCHAR));
	    type2 = CONFIG_TYPE_HOST;
	    cfgfile_load (p, tmp, &type2, 1);
	}
    }
end:
    recursive--;
    fixup_prefs (p);
    return v;
}

void cfgfile_backup (const TCHAR *path)
{
    TCHAR dpath[MAX_DPATH];

    fetch_configurationpath (dpath, sizeof (dpath) / sizeof (TCHAR));
    _tcscat (dpath, L"configuration.backup");
    my_unlink (dpath);
    my_rename (path, dpath);
}

int cfgfile_save (struct uae_prefs *p, const TCHAR *filename, int type)
{
    struct zfile *fh;

    cfgfile_backup (filename);
    fh = zfile_fopen (filename, unicode_config ? L"w, ccs=UTF-8" : L"w");
    if (! fh)
	return 0;

    if (!type)
	type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
    cfgfile_save_options (fh, p, type);
    zfile_fclose (fh);
    return 1;
}

int cfgfile_get_description (const TCHAR *filename, TCHAR *description, TCHAR *hostlink, TCHAR *hardwarelink, int *type)
{
    int result = 0;
    struct uae_prefs *p = xmalloc (sizeof (struct uae_prefs));

    p->description[0] = 0;
    p->config_host_path[0] = 0;
    p->config_hardware_path[0] = 0;
    if (cfgfile_load_2 (p, filename, 0, type)) {
	result = 1;
	if (description)
	    _tcscpy (description, p->description);
	if (hostlink)
	    _tcscpy (hostlink, p->config_host_path);
	if (hardwarelink)
	    _tcscpy (hardwarelink, p->config_hardware_path);
    }
    xfree (p);
    return result;
}

int cfgfile_configuration_change (int v)
{
    static int mode;
    if (v >= 0)
	mode = v;
    return mode;
}

void cfgfile_show_usage (void)
{
    int i;
    write_log (L"UAE Configuration Help:\n" \
	       L"=======================\n");
    for (i = 0; i < sizeof opttable / sizeof *opttable; i++)
	write_log (L"%s: %s\n", opttable[i].config_label, opttable[i].config_help);
}

/* This implements the old commandline option parsing. I've re-added this
   because the new way of doing things is painful for me (it requires me
   to type a couple hundred characters when invoking UAE).  The following
   is far less annoying to use.  */
static void parse_gfx_specs (struct uae_prefs *p, const TCHAR *spec)
{
    TCHAR *x0 = my_strdup (spec);
    TCHAR *x1, *x2;

    x1 = _tcschr (x0, ':');
    if (x1 == 0)
	goto argh;
    x2 = _tcschr (x1+1, ':');
    if (x2 == 0)
	goto argh;
    *x1++ = 0; *x2++ = 0;

    p->gfx_size_win.width = p->gfx_size_fs.width = _tstoi (x0);
    p->gfx_size_win.height = p->gfx_size_fs.height = _tstoi (x1);
    p->gfx_resolution = _tcschr (x2, 'l') != 0 ? 1 : 0;
    p->gfx_xcenter = _tcschr (x2, 'x') != 0 ? 1 : _tcschr (x2, 'X') != 0 ? 2 : 0;
    p->gfx_ycenter = _tcschr (x2, 'y') != 0 ? 1 : _tcschr (x2, 'Y') != 0 ? 2 : 0;
    p->gfx_linedbl = _tcschr (x2, 'd') != 0;
    p->gfx_linedbl += 2 * (_tcschr (x2, 'D') != 0);
    p->gfx_afullscreen = _tcschr (x2, 'a') != 0;
    p->gfx_pfullscreen = _tcschr (x2, 'p') != 0;

    if (p->gfx_linedbl == 3) {
	write_log (L"You can't use both 'd' and 'D' modifiers in the display mode specification.\n");
    }

    free (x0);
    return;

    argh:
    write_log (L"Bad display mode specification.\n");
    write_log (L"The format to use is: \"width:height:modifiers\"\n");
    write_log (L"Type \"uae -h\" for detailed help.\n");
    free (x0);
}

static void parse_sound_spec (struct uae_prefs *p, const TCHAR *spec)
{
    TCHAR *x0 = my_strdup (spec);
    TCHAR *x1, *x2 = NULL, *x3 = NULL, *x4 = NULL, *x5 = NULL;

    x1 = _tcschr (x0, ':');
    if (x1 != NULL) {
	*x1++ = '\0';
	x2 = _tcschr (x1 + 1, ':');
	if (x2 != NULL) {
	    *x2++ = '\0';
	    x3 = _tcschr (x2 + 1, ':');
	    if (x3 != NULL) {
		*x3++ = '\0';
		x4 = _tcschr (x3 + 1, ':');
		if (x4 != NULL) {
		    *x4++ = '\0';
		    x5 = _tcschr (x4 + 1, ':');
		}
	    }
	}
    }
    p->produce_sound = _tstoi (x0);
    if (x1) {
	p->sound_stereo_separation = 0;
	if (*x1 == 'S') {
	    p->sound_stereo = SND_STEREO;
	    p->sound_stereo_separation = 7;
	} else if (*x1 == 's')
	    p->sound_stereo = SND_STEREO;
	else
	    p->sound_stereo = SND_MONO;
    }
    if (x3)
	p->sound_freq = _tstoi (x3);
    if (x4)
	p->sound_maxbsiz = _tstoi (x4);
    free (x0);
}


static void parse_joy_spec (struct uae_prefs *p, const TCHAR *spec)
{
    int v0 = 2, v1 = 0;
    if (_tcslen(spec) != 2)
	goto bad;

    switch (spec[0]) {
     case '0': v0 = JSEM_JOYS; break;
     case '1': v0 = JSEM_JOYS + 1; break;
     case 'M': case 'm': v0 = JSEM_MICE; break;
     case 'A': case 'a': v0 = JSEM_KBDLAYOUT; break;
     case 'B': case 'b': v0 = JSEM_KBDLAYOUT + 1; break;
     case 'C': case 'c': v0 = JSEM_KBDLAYOUT + 2; break;
     default: goto bad;
    }

    switch (spec[1]) {
     case '0': v1 = JSEM_JOYS; break;
     case '1': v1 = JSEM_JOYS + 1; break;
     case 'M': case 'm': v1 = JSEM_MICE; break;
     case 'A': case 'a': v1 = JSEM_KBDLAYOUT; break;
     case 'B': case 'b': v1 = JSEM_KBDLAYOUT + 1; break;
     case 'C': case 'c': v1 = JSEM_KBDLAYOUT + 2; break;
     default: goto bad;
    }
    if (v0 == v1)
	goto bad;
    /* Let's scare Pascal programmers */
    if (0)
bad:
    write_log (L"Bad joystick mode specification. Use -J xy, where x and y\n"
	     L"can be 0 for joystick 0, 1 for joystick 1, M for mouse, and\n"
	     L"a, b or c for different keyboard settings.\n");

    p->jports[0].id = v0;
    p->jports[1].id = v1;
}

static void parse_filesys_spec (struct uae_prefs *p, int readonly, const TCHAR *spec)
{
    TCHAR buf[256];
    TCHAR *s2;

    _tcsncpy (buf, spec, 255); buf[255] = 0;
    s2 = _tcschr (buf, ':');
    if (s2) {
	*s2++ = '\0';
#ifdef __DOS__
	{
	    TCHAR *tmp;

	    while ((tmp = _tcschr (s2, '\\')))
		*tmp = '/';
	}
#endif
#ifdef FILESYS
	add_filesys_config (p, -1, NULL, buf, s2, readonly, 0, 0, 0, 0, 0, 0, 0, 0);
#endif
    } else {
	write_log (L"Usage: [-m | -M] VOLNAME:mount_point\n");
    }
}

static void parse_hardfile_spec (struct uae_prefs *p, const TCHAR *spec)
{
    TCHAR *x0 = my_strdup (spec);
    TCHAR *x1, *x2, *x3, *x4;

    x1 = _tcschr (x0, ':');
    if (x1 == NULL)
	goto argh;
    *x1++ = '\0';
    x2 = _tcschr (x1 + 1, ':');
    if (x2 == NULL)
	goto argh;
    *x2++ = '\0';
    x3 = _tcschr (x2 + 1, ':');
    if (x3 == NULL)
	goto argh;
    *x3++ = '\0';
    x4 = _tcschr (x3 + 1, ':');
    if (x4 == NULL)
	goto argh;
    *x4++ = '\0';
#ifdef FILESYS
    add_filesys_config (p, -1, NULL, NULL, x4, 0, _tstoi (x0), _tstoi (x1), _tstoi (x2), _tstoi (x3), 0, 0, 0, 0);
#endif
    free (x0);
    return;

 argh:
    free (x0);
    write_log (L"Bad hardfile parameter specified - type \"uae -h\" for help.\n");
    return;
}

static void parse_cpu_specs (struct uae_prefs *p, const TCHAR *spec)
{
    if (*spec < '0' || *spec > '4') {
	write_log (L"CPU parameter string must begin with '0', '1', '2', '3' or '4'.\n");
	return;
    }

    p->cpu_model = (*spec++) * 10 + 68000;
    p->address_space_24 = p->cpu_model < 68020;
    p->cpu_compatible = 0;
    while (*spec != '\0') {
	switch (*spec) {
	 case 'a':
	    if (p->cpu_model < 68020)
		write_log (L"In 68000/68010 emulation, the address space is always 24 bit.\n");
	    else if (p->cpu_model >= 68040)
		write_log (L"In 68040/060 emulation, the address space is always 32 bit.\n");
	    else
		p->address_space_24 = 1;
	    break;
	 case 'c':
	    if (p->cpu_model != 68000)
		write_log (L"The more compatible CPU emulation is only available for 68000\n"
			 L"emulation, not for 68010 upwards.\n");
	    else
		p->cpu_compatible = 1;
	    break;
	 default:
	    write_log (L"Bad CPU parameter specified - type \"uae -h\" for help.\n");
	    break;
	}
	spec++;
    }
}

/* Returns the number of args used up (0 or 1).  */
int parse_cmdline_option (struct uae_prefs *p, TCHAR c, const TCHAR *arg)
{
    struct strlist *u = xcalloc (sizeof (struct strlist), 1);
    const TCHAR arg_required[] = L"0123rKpImWSAJwNCZUFcblOdHRv";

    if (_tcschr (arg_required, c) && ! arg) {
	write_log (L"Missing argument for option `-%c'!\n", c);
	return 0;
    }

    u->option = malloc (2 * sizeof (TCHAR));
    u->option[0] = c;
    u->option[1] = 0;
    u->value = my_strdup (arg);
    u->next = p->all_lines;
    p->all_lines = u;

    switch (c) {
    case 'h': usage (); exit (0);

    case '0': _tcsncpy (p->df[0], arg, 255); p->df[0][255] = 0; break;
    case '1': _tcsncpy (p->df[1], arg, 255); p->df[1][255] = 0; break;
    case '2': _tcsncpy (p->df[2], arg, 255); p->df[2][255] = 0; break;
    case '3': _tcsncpy (p->df[3], arg, 255); p->df[3][255] = 0; break;
    case 'r': _tcsncpy (p->romfile, arg, 255); p->romfile[255] = 0; break;
    case 'K': _tcsncpy (p->romextfile, arg, 255); p->romextfile[255] = 0; break;
    case 'p': _tcsncpy (p->prtname, arg, 255); p->prtname[255] = 0; break;
	/*     case 'I': _tcsncpy (p->sername, arg, 255); p->sername[255] = 0; currprefs.use_serial = 1; break; */
    case 'm': case 'M': parse_filesys_spec (p, c == 'M', arg); break;
    case 'W': parse_hardfile_spec (p, arg); break;
    case 'S': parse_sound_spec (p, arg); break;
    case 'R': p->gfx_framerate = _tstoi (arg); break;
    case 'x': p->no_xhair = 1; break;
    case 'i': p->illegal_mem = 1; break;
    case 'J': parse_joy_spec (p, arg); break;

    case 't': p->test_drawing_speed = 1; break;
#ifdef USE_X11_GFX
    case 'L': p->x11_use_low_bandwidth = 1; break;
    case 'T': p->x11_use_mitshm = 1; break;
#endif
    case 'w': p->m68k_speed = _tstoi (arg); break;

	/* case 'g': p->use_gfxlib = 1; break; */
    case 'G': p->start_gui = 0; break;
    case 'D': p->start_debugger = 1; break;

    case 'n':
	if (_tcschr (arg, 'i') != 0)
	    p->immediate_blits = 1;
	break;

    case 'v':
	set_chipset_mask (p, _tstoi (arg));
	break;

    case 'C':
	parse_cpu_specs (p, arg);
	break;

    case 'Z':
	p->z3fastmem_size = _tstoi (arg) * 0x100000;
	break;

    case 'U':
	p->gfxmem_size = _tstoi (arg) * 0x100000;
	break;

    case 'F':
	p->fastmem_size = _tstoi (arg) * 0x100000;
	break;

    case 'b':
	p->bogomem_size = _tstoi (arg) * 0x40000;
	break;

    case 'c':
	p->chipmem_size = _tstoi (arg) * 0x80000;
	break;

    case 'l':
	if (0 == strcasecmp(arg, L"de"))
	    p->keyboard_lang = KBD_LANG_DE;
	else if (0 == strcasecmp(arg, L"dk"))
	    p->keyboard_lang = KBD_LANG_DK;
	else if (0 == strcasecmp(arg, L"us"))
	    p->keyboard_lang = KBD_LANG_US;
	else if (0 == strcasecmp(arg, L"se"))
	    p->keyboard_lang = KBD_LANG_SE;
	else if (0 == strcasecmp(arg, L"fr"))
	    p->keyboard_lang = KBD_LANG_FR;
	else if (0 == strcasecmp(arg, L"it"))
	    p->keyboard_lang = KBD_LANG_IT;
	else if (0 == strcasecmp(arg, L"es"))
	    p->keyboard_lang = KBD_LANG_ES;
	break;

    case 'O': parse_gfx_specs (p, arg); break;
    case 'd':
	if (_tcschr (arg, 'S') != NULL || _tcschr (arg, 's')) {
	    write_log (L"  Serial on demand.\n");
	    p->serial_demand = 1;
	}
	if (_tcschr (arg, 'P') != NULL || _tcschr (arg, 'p')) {
	    write_log (L"  Parallel on demand.\n");
	    p->parallel_demand = 1;
	}

	break;

    case 'H':
	p->color_mode = _tstoi (arg);
	if (p->color_mode < 0) {
	    write_log (L"Bad color mode selected. Using default.\n");
	    p->color_mode = 0;
	}
	break;
    default:
	write_log (L"Unknown option `-%c'!\n", c);
	break;
    }
    return !! _tcschr (arg_required, c);
}

void cfgfile_addcfgparam (TCHAR *line)
{
    struct strlist *u;
    TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

    if (!line) {
	struct strlist **ps = &temp_lines;
	while (*ps) {
	    struct strlist *s = *ps;
	    *ps = s->next;
	    xfree (s->value);
	    xfree (s->option);
	    xfree (s);
	}
	temp_lines = 0;
	return;
    }
    if (!cfgfile_separate_line (line, line1b, line2b))
	return;
    u = xcalloc (sizeof (struct strlist), 1);
    u->option = my_strdup (line1b);
    u->value = my_strdup (line2b);
    u->next = temp_lines;
    temp_lines = u;
}

static int getconfigstoreline (struct zfile *z, TCHAR *option, TCHAR *value)
{
    TCHAR tmp[CONFIG_BLEN * 2];
    int idx = 0;

    for (;;) {
	TCHAR b = 0;
	if (zfile_fread (&b, 1, sizeof (TCHAR), z) != 1)
	    return 0;
	tmp[idx++] = b;
	tmp[idx] = 0;
	if (b == '\n' || b == 0)
	    break;
    }
    return cfgfile_separate_line (tmp, option, value);
}

#if 0
static int cfgfile_handle_custom_event (TCHAR *custom, int mode)
{
    TCHAR option[CONFIG_BLEN], value[CONFIG_BLEN];
    TCHAR option2[CONFIG_BLEN], value2[CONFIG_BLEN];
    TCHAR *tmp, *p, *nextp;
    struct zfile *configstore = NULL;
    int cnt = 0, cnt_ok = 0;

    if (!mode) {
	uae_u8 zero = 0;
	configstore = zfile_fopen_empty ("configstore", 50000);
	cfgfile_save_options (configstore, &currprefs, 0);
	cfg_write (&zero, configstore);
    }

    nextp = NULL;
    tmp = p = xcalloc (_tcslen (custom) + 2, 1);
    _tcscpy (tmp, custom);
    while (p && *p) {
	if (*p == '\"') {
	    TCHAR *p2;
	    p++;
	    p2 = p;
	    while (*p2 != '\"' && *p2 != 0)
		p2++;
	    if (*p2 == '\"') {
		*p2++ = 0;
		nextp = p2 + 1;
		if (*nextp == ' ')
		    nextp++;
	    }
	}
	if (cfgfile_separate_line (p, option, value)) {
	    cnt++;
	    if (mode) {
		cfgfile_parse_option (&changed_prefs, option, value, 0);
	    } else {
		zfile_fseek (configstore, 0, SEEK_SET);
		for (;;) {
		    if (!getconfigstoreline (configstore, option2, value2))
			break;
		    if (!_tcscmpi (option, option2) && !_tcscmpi (value, value2)) {
			cnt_ok++;
			break;
		    }
		}
	    }
	}
	p = nextp;
    }
    xfree (tmp);
    zfile_fclose (configstore);
    if (cnt > 0 && cnt == cnt_ok)
	return 1;
    return 0;
}
#endif

int cmdlineparser (TCHAR *s, TCHAR *outp[], int max)
{
    int j, cnt = 0;
    int slash = 0;
    int quote = 0;
    TCHAR tmp1[MAX_DPATH];
    TCHAR *prev;
    int doout;

    doout = 0;
    prev = s;
    j = 0;
    while (cnt < max) {
	TCHAR c = *s++;
	if (!c)
	    break;
	if (c < 32)
	    continue;
	if (c == '\\')
	    slash = 1;
	if (!slash && c == '"') {
	    if (quote) {
		quote = 0;
		doout = 1;
	    } else {
		quote = 1;
		j = -1;
	    }
	}
	if (!quote && c == ' ')
	    doout = 1;
	if (!doout) {
	    if (j >= 0) {
		tmp1[j] = c;
		tmp1[j + 1] = 0;
	    }
	    j++;
	}
	if (doout) {
	    outp[cnt++] = my_strdup (tmp1);
	    tmp1[0] = 0;
	    doout = 0;
	    j = 0;
	}
	slash = 0;
    }
    if (j > 0 && cnt < max)
	outp[cnt++] = my_strdup (tmp1);
    return cnt;
}

#define UAELIB_MAX_PARSE 100

uae_u32 cfgfile_modify (uae_u32 index, TCHAR *parms, uae_u32 size, TCHAR *out, uae_u32 outsize)
{
    TCHAR *p;
    TCHAR *argc[UAELIB_MAX_PARSE];
    int argv, i;
    uae_u32 err;
    TCHAR zero = 0;
    static struct zfile *configstore;
    static TCHAR *configsearch;
    static int configsearchfound;

    err = 0;
    argv = 0;
    p = 0;
    if (index != 0xffffffff) {
	if (!configstore) {
	    err = 20;
	    goto end;
	}
	if (configsearch) {
	    TCHAR tmp[CONFIG_BLEN];
	    int j = 0;
	    TCHAR *in = configsearch;
	    int inlen = _tcslen (configsearch);
	    int joker = 0;

	    if (in[inlen - 1] == '*') {
		joker = 1;
		inlen--;
	    }

	    for (;;) {
		uae_u8 b = 0;

		if (zfile_fread (&b, 1, 1, configstore) != 1) {
		    err = 10;
		    if (configsearch)
			err = 5;
		    if (configsearchfound)
			err = 0;
		    goto end;
		}
		if (j >= sizeof (tmp) / sizeof (TCHAR) - 1)
		    j = sizeof (tmp) / sizeof (TCHAR) - 1;
		if (b == 0) {
		    err = 10;
		    if (configsearch)
			err = 5;
		    if (configsearchfound)
			err = 0;
		    goto end;
		}
		if (b == '\n') {
		    if (configsearch && !_tcsncmp (tmp, in, inlen) &&
			((inlen > 0 && _tcslen (tmp) > inlen && tmp[inlen] == '=') || (joker))) {
			TCHAR *p;
			if (joker)
			    p = tmp - 1;
			else
			    p = _tcschr (tmp, '=');
			if (p) {
			    for (i = 0; i < outsize - 1; i++) {
				TCHAR b = *++p;
				out[i] = b;
				out[i + 1] = 0;
				if (!b)
				    break;
			    }
			}
			err = 0xffffffff;
			configsearchfound++;
			goto end;
		    }
		    index--;
		    j = 0;
		} else {
		    tmp[j++] = b;
		    tmp[j] = 0;
		}
	    }
	}
	err = 0xffffffff;
	for (i = 0; i < outsize - 1; i++) {
	    uae_u8 b = 0;
	    if (zfile_fread (&b, 1, 1, configstore) != 1)
		err = 0;
	    if (b == 0)
		err = 0;
	    if (b == '\n')
		b = 0;
	    out[i] = b;
	    out[i + 1] = 0;
	    if (!b)
		break;
	}
	goto end;
    }

    if (size > 10000)
	return 10;
    argv = cmdlineparser (parms, argc, UAELIB_MAX_PARSE);

    if (argv <= 1 && index == 0xffffffff) {
	zfile_fclose (configstore);
	xfree (configsearch);
	configstore = zfile_fopen_empty (L"configstore", 50000);
	configsearch = NULL;
	if (argv > 0 && _tcslen (argc[0]) > 0)
	    configsearch = my_strdup (argc[0]);
	if (!configstore) {
	    err = 20;
	    goto end;
	}
	zfile_fseek (configstore, 0, SEEK_SET);
	uaeconfig++;
	cfgfile_save_options (configstore, &currprefs, 0);
	uaeconfig--;
	cfg_write (&zero, configstore);
	zfile_fseek (configstore, 0, SEEK_SET);
	err = 0xffffffff;
	configsearchfound = 0;
	goto end;
    }

    for (i = 0; i < argv; i++) {
	if (i + 2 <= argv) {
	    if (!inputdevice_uaelib (argc[i], argc[i + 1])) {
		if (!cfgfile_parse_option (&changed_prefs, argc[i], argc[i + 1], 0)) {
		    err = 5;
		    break;
		}
	    }
	    set_special (&regs, SPCFLAG_BRK);
	    i++;
	}
    }
end:
    for (i = 0; i < argv; i++)
	xfree (argc[i]);
    xfree (p);
    return err;
}

uae_u32 cfgfile_uaelib_modify (uae_u32 index, uae_u32 parms, uae_u32 size, uae_u32 out, uae_u32 outsize)
{
    uae_char *p, *parms_p = NULL, *parms_out = NULL;
    int i, ret;
    TCHAR *out_p = NULL, *parms_in = NULL;

    put_byte (out, 0);
    parms_p = xmalloc (size + 1);
    if (!parms_p) {
	ret = 10;
	goto end;
    }
    out_p = xmalloc ((outsize + 1) * sizeof (TCHAR));
    if (!out_p) {
	ret = 10;
	goto end;
    }
    p = parms_p;
    for (i = 0; i < size; i++) {
	p[i] = get_byte (parms + i);
	if (p[i] == 10 || p[i] == 13 || p[i] == 0)
	    break;
    }
    p[i] = 0;
    out_p[0] = 0;
    parms_in = au (parms_p);
    ret = cfgfile_modify (index, parms_in, size, out_p, outsize);
    xfree (parms_in);
    parms_out = ua (out_p);
    p = parms_out;
    for (i = 0; i < outsize - 1; i++) {
	uae_u8 b = *p++;
	put_byte (out + i, b);
	put_byte (out + i + 1, 0);
	if (!b)
	    break;
    }
    xfree (parms_out);
end:
    xfree (out_p);
    xfree (parms_p);
    return ret;
}

uae_u32 cfgfile_uaelib (int mode, uae_u32 name, uae_u32 dst, uae_u32 maxlen)
{
    TCHAR tmp[CONFIG_BLEN];
    int i;
    struct strlist *sl;

    if (mode)
	return 0;

    for (i = 0; i < sizeof (tmp) / sizeof (TCHAR); i++) {
	tmp[i] = get_byte (name + i);
	if (tmp[i] == 0)
	    break;
    }
    tmp[sizeof(tmp) / sizeof (TCHAR) - 1] = 0;
    if (tmp[0] == 0)
	return 0;
    for (sl = currprefs.all_lines; sl; sl = sl->next) {
	if (!strcasecmp (sl->option, tmp))
	    break;
    }

    if (sl) {
	char *s = ua (sl->value);
	for (i = 0; i < maxlen; i++) {
	    put_byte (dst + i, s[i]);
	    if (s[i] == 0)
		break;
	}
	xfree (s);
	return dst;
    }
    return 0;
}

uae_u8 *restore_configuration (uae_u8 *src)
{
    TCHAR *s = au (src);
    //write_log (s);
    xfree (s);
    src += strlen (src) + 1;
    return src;
}

uae_u8 *save_configuration (int *len)
{
    int tmpsize = 30000;
    uae_u8 *dstbak, *dst, *p;
    int index = -1;

    dstbak = dst = malloc (tmpsize * sizeof (TCHAR));
    p = dst;
    for (;;) {
	TCHAR tmpout[256];
	int ret;
	tmpout[0] = 0;
	ret = cfgfile_modify (index, L"*", 1, tmpout, sizeof (tmpout) / sizeof (TCHAR));
	index++;
	if (_tcslen (tmpout) > 0) {
	    char *out;
	    if (!_tcsncmp (tmpout, L"input.", 6))
		continue;
	    out = ua (tmpout);
	    strcpy (p, out);
	    xfree (out);
	    strcat (p, "\n");
	    p += strlen (p);
	    if (p - dstbak >= tmpsize - sizeof (tmpout))
		break;
	}
	if (ret >= 0)
	    break;
    }
    *len = p - dstbak + 1;
    return dstbak;
}


static int configure_rom (struct uae_prefs *p, int *rom, int msg)
{
    struct romdata *rd;
    TCHAR *path = 0;
    int i;

    i = 0;
    while (rom[i] >= 0) {
	rd = getromdatabyid (rom[i]);
	if (!rd) {
	    i++;
	    continue;
	}
	path = romlist_get (rd);
	if (path)
	    break;
	i++;
    }
    if (!path) {
	if (msg)
	    romwarning(rom);
	return 0;
    }
    if (rd->type & (ROMTYPE_KICK | ROMTYPE_KICKCD32))
	_tcscpy (p->romfile, path);
    if (rd->type & (ROMTYPE_EXTCD32 | ROMTYPE_EXTCDTV | ROMTYPE_ARCADIABIOS))
	_tcscpy (p->romextfile, path);
    if (rd->type & (ROMTYPE_CD32CART | ROMTYPE_ARCADIAGAME | ROMTYPE_HRTMON | ROMTYPE_XPOWER | ROMTYPE_NORDIC | ROMTYPE_AR | ROMTYPE_SUPERIV))
	_tcscpy (p->cartfile, path);
    return 1;
}

static void default_prefs_mini (struct uae_prefs *p, int type)
{
    _tcscpy (p->description, L"UAE default A500 configuration");

    p->nr_floppies = 1;
    p->dfxtype[0] = DRV_35_DD;
    p->dfxtype[1] = DRV_NONE;
    p->cpu_model = 68000;
    p->address_space_24 = 1;
    p->chipmem_size = 0x00080000;
    p->bogomem_size = 0x00080000;
}

#include "sounddep/sound.h"

void default_prefs (struct uae_prefs *p, int type)
{
    int i;
    int roms[] = { 6, 7, 8, 9, 10, 14, 5, 4, 3, 2, 1, -1 };
    uae_u8 zero = 0;
    struct zfile *f;

    memset (p, 0, sizeof (*p));
    _tcscpy (p->description, L"UAE default configuration");
    p->config_hardware_path[0] = 0;
    p->config_host_path[0] = 0;

    p->gfx_scandoubler = 0;
    p->start_gui = 1;
    p->start_debugger = 0;

    p->all_lines = 0;
    /* Note to porters: please don't change any of these options! UAE is supposed
     * to behave identically on all platforms if possible.
     * (TW says: maybe it is time to update default config..) */
    p->illegal_mem = 0;
    p->no_xhair = 0;
    p->use_serial = 0;
    p->serial_demand = 0;
    p->serial_hwctsrts = 1;
    p->parallel_demand = 0;
    p->parallel_postscript_emulation = 0;
    p->parallel_postscript_detection = 0;
    p->parallel_autoflush_time = 5;
    p->ghostscript_parameters[0] = 0;
    p->uae_hide = 0;

    memset (&p->jports[0], 0, sizeof (struct jport));
    memset (&p->jports[1], 0, sizeof (struct jport));
    p->jports[0].id = JSEM_MICE;
    p->jports[1].id = JSEM_KBDLAYOUT;
    p->keyboard_lang = KBD_LANG_US;

    p->produce_sound = 3;
    p->sound_stereo = SND_STEREO;
    p->sound_stereo_separation = 7;
    p->sound_mixed_stereo_delay = 0;
    p->sound_freq = DEFAULT_SOUND_FREQ;
    p->sound_maxbsiz = DEFAULT_SOUND_MAXB;
    p->sound_latency = 100;
    p->sound_interpol = 1;
    p->sound_filter = FILTER_SOUND_EMUL;
    p->sound_filter_type = 0;
    p->sound_auto = 1;

    p->comptrustbyte = 0;
    p->comptrustword = 0;
    p->comptrustlong = 0;
    p->comptrustnaddr= 0;
    p->compnf = 1;
    p->comp_hardflush = 0;
    p->comp_constjump = 1;
    p->comp_oldsegv = 0;
    p->compfpu = 1;
    p->fpu_strict = 0;
    p->cachesize = 0;
    p->avoid_cmov = 0;
    p->avoid_dga = 0;
    p->avoid_vid = 0;
    p->comp_midopt = 0;
    p->comp_lowopt = 0;
    p->override_dga_address = 0;

    for (i = 0;i < 10; i++)
	p->optcount[i] = -1;
    p->optcount[0] = 4;	/* How often a block has to be executed before it is translated */
    p->optcount[1] = 0;	/* How often to use the naive translation */
    p->optcount[2] = 0;
    p->optcount[3] = 0;
    p->optcount[4] = 0;
    p->optcount[5] = 0;

    p->gfx_framerate = 1;
    p->gfx_autoframerate = 50;
    p->gfx_size_fs.width = 800;
    p->gfx_size_fs.height = 600;
    p->gfx_size_win.width = 720;
    p->gfx_size_win.height = 568;
    for (i = 0; i < 4; i++) {
	p->gfx_size_fs_xtra[i].width = 0;
	p->gfx_size_fs_xtra[i].height = 0;
	p->gfx_size_win_xtra[i].width = 0;
	p->gfx_size_win_xtra[i].height = 0;
    }
    p->gfx_resolution = 1;
    p->gfx_linedbl = 1;
    p->gfx_afullscreen = 0;
    p->gfx_pfullscreen = 0;
    p->gfx_xcenter = 0; p->gfx_ycenter = 0;
    p->gfx_xcenter_pos = -1; p->gfx_ycenter_pos = -1;
    p->gfx_xcenter_size = -1; p->gfx_ycenter_size = -1;
    p->gfx_max_horizontal = RES_HIRES;
    p->gfx_max_vertical = 1;
    p->color_mode = 2;
    p->gfx_blackerthanblack = 0;

    p->x11_use_low_bandwidth = 0;
    p->x11_use_mitshm = 0;
    p->x11_hide_cursor = 1;

    p->svga_no_linear = 0;

    p->curses_reverse_video = 0;

    target_default_options (p, type);

    p->immediate_blits = 0;
    p->collision_level = 2;
    p->leds_on_screen = 0;
    p->keyboard_leds_in_use = 0;
    p->keyboard_leds[0] = p->keyboard_leds[1] = p->keyboard_leds[2] = 0;
    p->scsi = 0;
    p->uaeserial = 0;
    p->cpu_idle = 0;
    p->catweasel = 0;
    p->tod_hack = 0;
    p->maprom = 0;
    p->filesys_no_uaefsdb = 0;
    p->filesys_custom_uaefsdb = 1;
    p->picasso96_nocustom = 1;
    p->cart_internal = 1;
    p->sana2 = 0;

    p->cs_compatible = 1;
    p->cs_rtc = 2;
    p->cs_df0idhw = 1;
    p->cs_a1000ram = 0;
    p->cs_fatgaryrev = -1;
    p->cs_ramseyrev = -1;
    p->cs_agnusrev = -1;
    p->cs_deniserev = -1;
    p->cs_mbdmac = 0;
    p->cs_a2091 = 0;
    p->cs_a4091 = 0;
    p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 0;
    p->cs_cdtvcd = p->cs_cdtvram = p->cs_cdtvcard = 0;
    p->cs_pcmcia = 0;
    p->cs_ksmirror_e0 = 1;
    p->cs_ksmirror_a8 = 0;
    p->cs_ciaoverlay = 1;
    p->cs_ciaatod = 0;
    p->cs_df0idhw = 1;
    p->cs_slowmemisfast = 0;
    p->cs_resetwarning = 1;

    p->gfx_filter = 0;
    p->gfx_filtershader[0] = 0;
    p->gfx_filter_horiz_zoom_mult = 0;
    p->gfx_filter_vert_zoom_mult = 0;
    p->gfx_filter_filtermode = 0;
    p->gfx_filter_scanlineratio = (1 << 4) | 1;
    p->gfx_filter_keep_aspect = 0;
    p->gfx_filter_autoscale = 0;

    _tcscpy (p->df[0], L"df0.adf");
    _tcscpy (p->df[1], L"df1.adf");
    _tcscpy (p->df[2], L"df2.adf");
    _tcscpy (p->df[3], L"df3.adf");

    configure_rom (p, roms, 0);
    _tcscpy (p->romextfile, L"");
    _tcscpy (p->flashfile, L"");
    _tcscpy (p->cartfile, L"");

    _tcscpy (p->path_rom, L"./");
    _tcscpy (p->path_floppy, L"./");
    _tcscpy (p->path_hardfile, L"./");

    p->prtname[0] = 0;
    p->sername[0] = 0;

    p->fpu_model = 0;
    p->cpu_model = 68000;
    p->mmu_model = 0;
    p->cpu060_revision = 6;
    p->fpu_revision = -1;
    p->m68k_speed = 0;
    p->cpu_compatible = 1;
    p->address_space_24 = 1;
    p->cpu_cycle_exact = 0;
    p->blitter_cycle_exact = 0;
    p->chipset_mask = CSMASK_ECS_AGNUS;
    p->genlock = 0;
    p->ntscmode = 0;

    p->fastmem_size = 0x00000000;
    p->mbresmem_low_size = 0x00000000;
    p->mbresmem_high_size = 0x00000000;
    p->z3fastmem_size = 0x00000000;
    p->z3fastmem2_size = 0x00000000;
    p->z3fastmem_start = 0x10000000;
    p->chipmem_size = 0x00080000;
    p->bogomem_size = 0x00080000;
    p->gfxmem_size = 0x00000000;
    p->custom_memory_addrs[0] = 0;
    p->custom_memory_sizes[0] = 0;
    p->custom_memory_addrs[1] = 0;
    p->custom_memory_sizes[1] = 0;

    p->nr_floppies = 2;
    p->dfxtype[0] = DRV_35_DD;
    p->dfxtype[1] = DRV_35_DD;
    p->dfxtype[2] = DRV_NONE;
    p->dfxtype[3] = DRV_NONE;
    p->floppy_speed = 100;
    p->floppy_write_length = 0;
    p->dfxclickvolume = 33;

    p->statecapturebuffersize = 20 * 1024 * 1024;
    p->statecapturerate = 5 * 50;
    p->statecapture = 0;

#ifdef UAE_MINI
    default_prefs_mini (p, 0);
#endif

    p->input_tablet = TABLET_OFF;
    p->input_magic_mouse = 0;
    p->input_magic_mouse_cursor = 0;

    inputdevice_default_prefs (p);

    zfile_fclose (default_file);
    default_file = NULL;
    f = zfile_fopen_empty (L"configstore", 100000);
    if (f) {
	uaeconfig++;
	cfgfile_save_options (f, p, 0);
	uaeconfig--;
	cfg_write (&zero, f);
	default_file = f;
    }
}

static void buildin_default_prefs_68020 (struct uae_prefs *p)
{
    p->cpu_model = 68020;
    p->address_space_24 = 1;
    p->cpu_compatible = 1;
    p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;
    p->chipmem_size = 0x200000;
    p->bogomem_size = 0;
    p->m68k_speed = -1;
}

static void buildin_default_host_prefs (struct uae_prefs *p)
{
#if 0
    p->sound_filter = FILTER_SOUND_OFF;
    p->sound_stereo = SND_STEREO;
    p->sound_stereo_separation = 7;
    p->sound_mixed_stereo = 0;
#endif
}

static void buildin_default_prefs (struct uae_prefs *p)
{
    buildin_default_host_prefs (p);

    p->dfxtype[0] = DRV_35_DD;
    if (p->nr_floppies != 1 && p->nr_floppies != 2)
	p->nr_floppies = 2;
    p->dfxtype[1] = p->nr_floppies >= 2 ? DRV_35_DD : DRV_NONE;
    p->dfxtype[2] = DRV_NONE;
    p->dfxtype[3] = DRV_NONE;
    p->floppy_speed = 100;

    p->fpu_model = 0;
    p->cpu_model = 68000;
    p->cpu060_revision = 1;
    p->fpu_revision = -1;
    p->m68k_speed = 0;
    p->cpu_compatible = 1;
    p->address_space_24 = 1;
    p->cpu_cycle_exact = 0;
    p->blitter_cycle_exact = 0;
    p->chipset_mask = CSMASK_ECS_AGNUS;
    p->immediate_blits = 0;
    p->collision_level = 2;
    p->produce_sound = 3;
    p->scsi = 0;
    p->uaeserial = 0;
    p->cpu_idle = 0;
    p->catweasel = 0;
    p->tod_hack = 0;
    p->maprom = 0;
    p->cachesize = 0;
    p->socket_emu = 0;

    p->chipmem_size = 0x00080000;
    p->bogomem_size = 0x00080000;
    p->fastmem_size = 0x00000000;
    p->mbresmem_low_size = 0x00000000;
    p->mbresmem_high_size = 0x00000000;
    p->z3fastmem_size = 0x00000000;
    p->z3fastmem2_size = 0x00000000;
    p->gfxmem_size = 0x00000000;

    p->cs_rtc = 0;
    p->cs_a1000ram = 0;
    p->cs_fatgaryrev = -1;
    p->cs_ramseyrev = -1;
    p->cs_agnusrev = -1;
    p->cs_deniserev = -1;
    p->cs_mbdmac = 0;
    p->cs_a2091 = 0;
    p->cs_a4091 = 0;
    p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 0;
    p->cs_cdtvcd = p->cs_cdtvram = p->cs_cdtvcard = 0;
    p->cs_ide = 0;
    p->cs_pcmcia = 0;
    p->cs_ksmirror_e0 = 1;
    p->cs_ksmirror_a8 = 0;
    p->cs_ciaoverlay = 1;
    p->cs_ciaatod = 0;
    p->cs_df0idhw = 1;
    p->cs_resetwarning = 0;

    _tcscpy (p->romfile, L"");
    _tcscpy (p->romextfile, L"");
    _tcscpy (p->flashfile, L"");
    _tcscpy (p->cartfile, L"");
    _tcscpy (p->amaxromfile, L"");
    p->prtname[0] = 0;
    p->sername[0] = 0;

    p->mountitems = 0;

    target_default_options (p, 1);
}

static void set_68020_compa (struct uae_prefs *p, int compa)
{
    if (compa == 0)
	p->blitter_cycle_exact = 1;
    if (compa > 0) {
	p->cpu_compatible = 0;
	p->address_space_24 = 0;
	p->cachesize = 8192;
    }
    if (compa > 1) {
	p->immediate_blits = 1;
	p->produce_sound = 2;
    }
}

/* 0: cycle-exact
 * 1: more compatible
 * 2: no more compatible, no 100% sound
 * 3: no more compatible, immediate blits, no 100% sound
 */

static void set_68000_compa (struct uae_prefs *p, int compa)
{
    switch (compa)
    {
	case 0:
	p->cpu_cycle_exact = p->blitter_cycle_exact = 1;
	break;
	case 1:
	break;
	case 2:
	p->cpu_compatible = 0;
	break;
	case 3:
	p->immediate_blits = 1;
	p->produce_sound = 2;
	p->cpu_compatible = 0;
	break;
    }
}

static int bip_a3000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[2];

    if (config == 2)
	roms[0] = 61;
    else if (config == 1)
	roms[0] = 71;
    else
	roms[0] = 59;
    roms[1] = -1;
    p->immediate_blits = 1;
    p->bogomem_size = 0;
    p->chipmem_size = 0x200000;
    p->cpu_model = 68030;
    p->fpu_model = 68882;
    p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    p->cpu_compatible = p->address_space_24 = 0;
    p->m68k_speed = -1;
    p->immediate_blits = 0;
    p->produce_sound = 2;
    p->cachesize = 8192;
    p->dfxtype[0] = DRV_35_HD;
    p->floppy_speed = 0;
    p->cpu_idle = 150;
    p->cs_compatible = CP_A3000;
    p->mbresmem_low_size = 8 * 1024 * 1024;
    built_in_chipset_prefs (p);
    p->cs_ciaatod = p->ntscmode ? 2 : 1;
    return configure_rom (p, roms, romcheck);
}
static int bip_a4000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[8];

    roms[0] = 16;
    roms[1] = 31;
    roms[2] = 13;
    roms[3] = 12;
    roms[4] = -1;
    p->immediate_blits = 1;
    p->bogomem_size = 0;
    p->chipmem_size = 0x200000;
    p->mbresmem_low_size = 8 * 1024 * 1024;
    p->cpu_model = 68030;
    p->fpu_model = 68882;
    if (config > 0)
	p->cpu_model = p->fpu_model = 68040;
    p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    p->cpu_compatible = p->address_space_24 = 0;
    p->m68k_speed = -1;
    p->immediate_blits = 0;
    p->produce_sound = 2;
    p->cachesize = 8192;
    p->dfxtype[0] = DRV_35_HD;
    p->dfxtype[1] = DRV_35_HD;
    p->floppy_speed = 0;
    p->cpu_idle = 150;
    p->cs_compatible = CP_A4000;
    built_in_chipset_prefs (p);
    p->cs_ciaatod = p->ntscmode ? 2 : 1;
    return configure_rom (p, roms, romcheck);
}
static int bip_a4000t (struct uae_prefs *p, int config, int compa, int romcheck)
{

    int roms[8];

    roms[0] = 16;
    roms[1] = 31;
    roms[2] = 13;
    roms[3] = -1;
    p->immediate_blits = 1;
    p->bogomem_size = 0;
    p->chipmem_size = 0x200000;
    p->mbresmem_low_size = 8 * 1024 * 1024;
    p->cpu_model = 68030;
    p->fpu_model = 68882;
    if (config > 0)
	p->cpu_model = p->fpu_model = 68040;
    p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    p->cpu_compatible = p->address_space_24 = 0;
    p->m68k_speed = -1;
    p->immediate_blits = 0;
    p->produce_sound = 2;
    p->cachesize = 8192;
    p->dfxtype[0] = DRV_35_HD;
    p->dfxtype[1] = DRV_35_HD;
    p->floppy_speed = 0;
    p->cpu_idle = 150;
    p->cs_compatible = CP_A4000T;
    built_in_chipset_prefs (p);
    p->cs_ciaatod = p->ntscmode ? 2 : 1;
    return configure_rom (p, roms, romcheck);
}

static int bip_a1000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[4];

    roms[0] = 24;
    roms[1] = 23;
    roms[2] = -1;
    p->chipset_mask = 0;
    p->bogomem_size = 0;
    p->sound_filter = FILTER_SOUND_ON;
    set_68000_compa (p, compa);
    p->dfxtype[1] = DRV_NONE;
    p->cs_compatible = CP_A1000;
    p->cs_slowmemisfast = 1;
    built_in_chipset_prefs (p);
    if (config > 0)
	p->cs_denisenoehb = 1;
    if (config > 1)
	p->chipmem_size = 0x40000;
    return configure_rom (p, roms, romcheck);
}

static int bip_cdtv (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[4];

    roms[0] = 6;
    roms[1] = 32;
    roms[2] = -1;
    if (!configure_rom (p, roms, romcheck))
	return 0;
    roms[0] = 20;
    roms[1] = 21;
    roms[2] = 22;
    roms[3] = -1;
    if (!configure_rom (p, roms, romcheck))
	return 0;
    p->bogomem_size = 0;
    p->chipmem_size = 0x100000;
    p->chipset_mask = CSMASK_ECS_AGNUS;
    p->cs_cdtvcd = p->cs_cdtvram = 1;
    if (config > 0)
	p->cs_cdtvcard = 64;
    p->cs_rtc = 1;
    p->nr_floppies = 0;
    p->dfxtype[0] = DRV_NONE;
    if (config > 0)
	p->dfxtype[0] = DRV_35_DD;
    p->dfxtype[1] = DRV_NONE;
    set_68000_compa (p, compa);
    p->cs_compatible = CP_CDTV;
    built_in_chipset_prefs (p);
    fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
    _tcscat (p->flashfile, L"cdtv.nvr");
    return 1;
}

static int bip_cd32 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[2];

    buildin_default_prefs_68020 (p);
    roms[0] = 64;
    roms[1] = -1;
    if (!configure_rom (p, roms, 0)) {
	roms[0] = 18;
	roms[1] = -1;
	if (!configure_rom (p, roms, romcheck))
	    return 0;
	roms[0] = 19;
	if (!configure_rom (p, roms, romcheck))
	    return 0;
    }
    if (config > 0) {
	roms[0] = 23;
	if (!configure_rom (p, roms, romcheck))
	    return 0;
    }
    p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 1;
    p->nr_floppies = 0;
    p->dfxtype[0] = DRV_NONE;
    p->dfxtype[1] = DRV_NONE;
    set_68020_compa (p, compa);
    p->cs_compatible = CP_CD32;
    built_in_chipset_prefs (p);
    fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
    _tcscat (p->flashfile, L"cd32.nvr");
    return 1;
}

static int bip_a1200 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[4];

    buildin_default_prefs_68020 (p);
    roms[0] = 11;
    roms[1] = 15;
    roms[2] = 31;
    roms[3] = -1;
    p->cs_rtc = 0;
    if (config == 1) {
	p->fastmem_size = 0x400000;
	p->cs_rtc = 2;
    }
    set_68020_compa (p, compa);
    p->cs_compatible = CP_A1200;
    built_in_chipset_prefs (p);
    return configure_rom (p, roms, romcheck);
}

static int bip_a600 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[4];

    roms[0] = 10;
    roms[1] = 9;
    roms[2] = 8;
    roms[3] = -1;
    p->bogomem_size = 0;
    p->chipmem_size = 0x100000;
    if (config > 0)
	p->cs_rtc = 1;
    if (config == 1)
	p->chipmem_size = 0x200000;
    if (config == 2)
	p->fastmem_size = 0x400000;
    p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    set_68000_compa (p, compa);
    p->cs_compatible = CP_A600;
    built_in_chipset_prefs (p);
    return configure_rom (p, roms, romcheck);
}

static int bip_a500p (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[2];

    roms[0] = 7;
    roms[1] = -1;
    p->bogomem_size = 0;
    p->chipmem_size = 0x100000;
    if (config > 0)
	p->cs_rtc = 1;
    if (config == 1)
	p->chipmem_size = 0x200000;
    if (config == 2)
	p->fastmem_size = 0x400000;
    p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    set_68000_compa (p, compa);
    p->cs_compatible = CP_A500P;
    built_in_chipset_prefs (p);
    return configure_rom (p, roms, romcheck);
}
static int bip_a500 (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[4];

    roms[0] = roms[1] = roms[2] = roms[3] = -1;
    switch (config)
    {
    case 0: // KS 1.3, OCS Agnus, 0.5M Chip + 0.5M Slow
	roms[0] = 6;
	roms[1] = 32;
	p->chipset_mask = 0;
	break;
    case 1: // KS 1.3, ECS Agnus, 0.5M Chip + 0.5M Slow
	roms[0] = 6;
	roms[1] = 32;
	break;
    case 2: // KS 1.3, ECS Agnus, 1.0M Chip
	roms[0] = 6;
	roms[1] = 32;
	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	break;
    case 3: // KS 1.3, OCS Agnus, 0.5M Chip
	roms[0] = 6;
	roms[1] = 32;
	p->bogomem_size = 0;
	p->chipset_mask = 0;
	p->cs_rtc = 0;
	p->dfxtype[1] = DRV_NONE;
	break;
    case 4: // KS 1.2, OCS Agnus, 0.5M Chip
	roms[0] = 5;
	roms[1] = 4;
	roms[2] = 3;
	p->bogomem_size = 0;
	p->chipset_mask = 0;
	p->cs_rtc = 0;
	p->dfxtype[1] = DRV_NONE;
	break;
    case 5: // KS 1.2, OCS Agnus, 0.5M Chip + 0.5M Slow
	roms[0] = 5;
	roms[1] = 4;
	roms[2] = 3;
	p->chipset_mask = 0;
	break;
    }
    set_68000_compa (p, compa);
    p->cs_compatible = CP_A500;
    built_in_chipset_prefs (p);
    return configure_rom (p, roms, romcheck);
}

static int bip_super (struct uae_prefs *p, int config, int compa, int romcheck)
{
    int roms[8];

    roms[0] = 46;
    roms[1] = 16;
    roms[2] = 31;
    roms[3] = 15;
    roms[4] = 14;
    roms[5] = 12;
    roms[6] = 11;
    roms[7] = -1;
    p->bogomem_size = 0;
    p->chipmem_size = 0x400000;
    p->z3fastmem_size = 8 * 1024 * 1024;
    p->gfxmem_size = 8 * 1024 * 1024;
    p->cpu_model = 68040;
    p->fpu_model = 68040;
    p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    p->cpu_compatible = p->address_space_24 = 0;
    p->m68k_speed = -1;
    p->immediate_blits = 1;
    p->produce_sound = 2;
    p->cachesize = 8192;
    p->dfxtype[0] = DRV_35_HD;
    p->dfxtype[1] = DRV_35_HD;
    p->floppy_speed = 0;
    p->cpu_idle = 150;
    p->scsi = 1;
    p->uaeserial = 1;
    p->socket_emu = 1;
    p->cart_internal = 0;
    p->picasso96_nocustom = 1;
    p->cs_compatible = 1;
    built_in_chipset_prefs (p);
    p->cs_ide = -1;
    p->cs_ciaatod = p->ntscmode ? 2 : 1;
    //_tcscat(p->flashfile, L"battclock.nvr");
    return configure_rom (p, roms, romcheck);
}

 static int bip_arcadia (struct uae_prefs *p, int config, int compa, int romcheck)
 {
    int roms[4];

    p->bogomem_size = 0;
    p->chipset_mask = 0;
    p->cs_rtc = 0;
    p->nr_floppies = 0;
    p->dfxtype[0] = DRV_NONE;
    p->dfxtype[1] = DRV_NONE;
    set_68000_compa (p, compa);
    p->cs_compatible = CP_A500;
    built_in_chipset_prefs (p);
    fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
    _tcscat (p->flashfile, L"arcadia.nvr");
    roms[0] = 5;
    roms[1] = 4;
    roms[2] = -1;
    if (!configure_rom (p, roms, romcheck))
	return 0;
    roms[0] = 49;
    roms[1] = 50;
    roms[2] = 51;
    roms[3] = -1;
    if (!configure_rom (p, roms, romcheck))
	return 0;
    return 1;
}

int built_in_prefs (struct uae_prefs *p, int model, int config, int compa, int romcheck)
{
    int v = 0, i;

    buildin_default_prefs (p);
    switch (model)
    {
    case 0:
	v = bip_a500 (p, config, compa, romcheck);
	break;
    case 1:
	v = bip_a500p (p, config, compa, romcheck);
	break;
    case 2:
	v = bip_a600 (p, config, compa, romcheck);
	break;
    case 3:
	v = bip_a1000 (p, config, compa, romcheck);
	break;
    case 4:
	v = bip_a1200 (p, config, compa, romcheck);
	break;
    case 5:
	v = bip_a3000 (p, config, compa, romcheck);
	break;
    case 6:
	v = bip_a4000 (p, config, compa, romcheck);
	break;
    case 7:
	v = bip_a4000t (p, config, compa, romcheck);
	break;
    case 8:
	v = bip_cd32 (p, config, compa, romcheck);
	break;
    case 9:
	v = bip_cdtv (p, config, compa, romcheck);
	break;
    case 10:
	v = bip_arcadia (p, config , compa, romcheck);
	break;
    case 11:
	v = bip_super (p, config, compa, romcheck);
	break;
    }
    for (i = 0; i < 4; i++) {
	if (p->dfxtype[i] < 0)
	    p->df[i][0] = DRV_35_DD;
    }
    return v;
}

int built_in_chipset_prefs (struct uae_prefs *p)
{
    if (!p->cs_compatible)
	return 1;

    p->cs_a1000ram = 0;
    p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 0;
    p->cs_cdtvcd = p->cs_cdtvram = 0;
    p->cs_fatgaryrev = -1;
    p->cs_ide = 0;
    p->cs_ramseyrev = -1;
    p->cs_deniserev = -1;
    p->cs_agnusrev = -1;
    p->cs_mbdmac = 0;
    p->cs_a2091 = 0;
    p->cs_pcmcia = 0;
    p->cs_ksmirror_e0 = 1;
    p->cs_ciaoverlay = 1;
    p->cs_ciaatod = 0;
    p->cs_df0idhw = 1;
    p->cs_resetwarning = 1;

    switch (p->cs_compatible)
    {
    case CP_GENERIC: // generic
	p->cs_rtc = 2;
	p->cs_fatgaryrev = 0;
	p->cs_ide = -1;
	p->cs_mbdmac = 1;
	p->cs_ramseyrev = 0x0f;
	break;
    case CP_CDTV: // CDTV
	p->cs_rtc = 1;
	p->cs_cdtvcd = p->cs_cdtvram = 1;
	p->cs_df0idhw = 1;
	p->cs_ksmirror_e0 = 0;
	break;
    case CP_CD32: // CD32
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 1;
	p->cs_ksmirror_e0 = 0;
	p->cs_ksmirror_a8 = 1;
	p->cs_ciaoverlay = 0;
	p->cs_resetwarning = 0;
	break;
    case CP_A500: // A500
	p->cs_df0idhw = 0;
	p->cs_resetwarning = 0;
	if (p->bogomem_size || p->chipmem_size > 1 || p->fastmem_size)
	    p->cs_rtc = 1;
	break;
    case CP_A500P: // A500+
	p->cs_rtc = 1;
	p->cs_resetwarning = 0;
	break;
    case CP_A600: // A600
	p->cs_rtc = 1;
	p->cs_ide = 1;
	p->cs_pcmcia = 1;
	p->cs_ksmirror_a8 = 1;
	p->cs_ciaoverlay = 0;
	p->cs_resetwarning = 0;
	break;
    case CP_A1000: // A1000
	p->cs_a1000ram = 1;
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	p->cs_ksmirror_e0 = 0;
	p->cs_rtc = 0;
	p->cs_agnusbltbusybug = 1;
	break;
    case CP_A1200: // A1200
	p->cs_ide = 1;
	p->cs_pcmcia = 1;
	p->cs_ksmirror_a8 = 1;
	p->cs_ciaoverlay = 0;
	break;
    case CP_A2000: // A2000
	p->cs_rtc = 1;
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	break;
    case CP_A3000: // A3000
	p->cs_rtc = 2;
	p->cs_fatgaryrev = 0;
	p->cs_ramseyrev = 0x0d;
	p->cs_mbdmac = 1;
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	break;
    case CP_A3000T: // A3000T
	p->cs_rtc = 2;
	p->cs_fatgaryrev = 0;
	p->cs_ramseyrev = 0x0d;
	p->cs_mbdmac = 1;
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	break;
    case CP_A4000: // A4000
	p->cs_rtc = 2;
	p->cs_fatgaryrev = 0;
	p->cs_ramseyrev = 0x0f;
	p->cs_ide = 2;
	p->cs_mbdmac = 0;
	p->cs_ksmirror_a8 = 1;
	p->cs_ciaoverlay = 0;
	break;
    case CP_A4000T: // A4000T
	p->cs_rtc = 2;
	p->cs_fatgaryrev = 0;
	p->cs_ramseyrev = 0x0f;
	p->cs_ide = 2;
	p->cs_mbdmac = 2;
	p->cs_ksmirror_a8 = 1;
	p->cs_ciaoverlay = 0;
	break;
    }
    return 1;
}
