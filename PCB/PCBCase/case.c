/* Make an OpendScad file from a kicad_pcb file */
/* (c) 2021-2022 Adrian Kennard Andrews & Arnold Ltd */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <popt.h>
#include <err.h>
#include <float.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

/* yet, all globals, what the hell */
int debug = 0;
int norender = 0;
int layerpcb = 0;
int layercase = 0;
int nohull = 0;
const char *pcbfile = NULL;
char *scadfile = NULL;
const char *modeldir = "PCBCase/models";
const char *ignore = NULL;
double pcbthickness = 0;
double casebase = 5;
double casetop = 5;
double casewall = 3;            /* margin/2 eats in to this  */
double overlap = 2;             /* Lip overlap */
double lip = 0;                 /* Lip offset */
double fit = 0.0;
double edge = 1;
double margin = 0.5;
double spacing = 0;
double delta = 0.01;
double hullcap = 1;
double hulledge = 1;
//Curve delta

/* strings from file, lots of common, so make a table */
int strn = 0;
const char **strs = NULL;       /* the object tags */
const char *
add_string (const char *s, const char *e)
{                               /* allocates a string */
   /* simplistic */
   int n;
   for (n = 0; n < strn; n++)
      if (strlen (strs[n]) == (int) (e - s) && !memcmp (strs[n], s, (int) (e - s)))
         return strs[n];
   strs = realloc (strs, (++strn) * sizeof (*strs));
   if (!strs)
      errx (1, "malloc");
   strs[n] = strndup (s, (int) (e - s));
   return strs[n];
}

typedef struct obj_s obj_t;
typedef struct value_s value_t;

struct value_s
{                               /* value */
   /* only one set */
   unsigned char isobj:1;       /* object */
   unsigned char isnum:1;       /* number */
   unsigned char isbool:1;      /* boolean */
   unsigned char istxt:1;       /* text */
   union
   {                            /* the value */
      obj_t *obj;
      double num;
      const char *txt;
      unsigned char bool:1;
   };
};

obj_t *pcb = NULL;

struct obj_s
{                               /* an object */
   const char *tag;             /* object tag */
   int valuen;                  /* number of values */
   value_t *values;             /* the values */
};

obj_t *
parse_obj (const char **pp, const char *e)
{                               /* Scan an object */
   const char *p = *pp;
   obj_t *pcb = malloc (sizeof (*pcb));
   if (p >= e)
      errx (1, "EOF");
   memset (pcb, 0, sizeof (*pcb));
   if (*p != '(')
      errx (1, "Expecting (\n%.20s\n", p);
   p++;
   if (p >= e)
      errx (1, "EOF");
   /* tag */
   const char *t = p;
   while (p < e && (isalnum (*p) || *p == '_'))
      p++;
   if (p == t)
      errx (1, "Expecting tag\n%.20s\n", t);
   pcb->tag = add_string (t, p);
   /* values */
   while (p < e)
   {
      while (p < e && isspace (*p))
         p++;
      if (*p == ')')
         break;
      pcb->values = realloc (pcb->values, (++(pcb->valuen)) * sizeof (*pcb->values));
      if (!pcb->values)
         errx (1, "malloc");
      value_t *value = pcb->values + pcb->valuen - 1;
      memset (value, 0, sizeof (*value));
      /* value */
      if (*p == '(')
      {
         value->isobj = 1;
         value->obj = parse_obj (&p, e);
         continue;
      }
      if (*p == '"')
      {                         /* quoted text */
         p++;
         t = p;
         while (p < e && *p != '"')
         {
            if (*p == '\\' && p[1])
               p++;
            p++;
         }
         if (p == e)
            errx (1, "EOF");
         value->istxt = 1;
         value->txt = add_string (t, p);
         p++;
         continue;
      }
      t = p;
      while (p < e && *p != ')' && *p != ' ')
         p++;
      if (p == e)
         errx (1, "EOF");
      /* work out some basic types */
      if ((p - t) == 4 && !memcmp (t, "true", (int) (p - t)))
      {
         value->isbool = 1;
         value->bool = 1;
         continue;;
      }
      if ((p - t) == 5 && !memcmp (t, "false", (int) (p - t)))
      {
         value->isbool = 1;
         continue;;
      }
      /* does it look like a value number */
      const char *q = t;
      if (q < p && *q == '-')
         q++;
      while (q < p && isdigit (*q))
         q++;
      if (q < p && *q == '.')
      {
         q++;
         while (q < p && isdigit (*q))
            q++;
      }
      if (q == p)
      {                         /* seems legit */
         char *val = strndup (t, q - t);
         double v = 0;
         if (sscanf (val, "%lf", &v) == 1)
         {                      /* safe as we know followed by space or close bracket and not EOF */
            value->isnum = 1;
            value->num = v;
            free (val);
            continue;
         }
         free (val);
      }
      /* assume string */
      value->istxt = 1;
      value->txt = add_string (t, p);
   }
   if (p >= e)
      errx (1, "EOF");
   if (*p != ')')
      errx (1, "Expecting )\n%.20s\n", p);
   p++;
   while (p < e && isspace (*p))
      p++;
   *pp = p;
   return pcb;
}

void
dump_obj (obj_t * o)
{
   printf ("(%s", o->tag);
   for (int n = 0; n < o->valuen; n++)
   {
      value_t *v = &o->values[n];
      if (v->isobj)
         dump_obj (v->obj);
      else if (v->istxt)
         printf (" \"%s\"", v->txt);
      else if (v->isnum)
         printf (" %lf", v->num);
      else if (v->isbool)
         printf (" %s", v->bool ? "true" : "false");
   }
   printf (")\n");
}

obj_t *
find_obj (obj_t * o, const char *tag, obj_t * prev)
{
   int n = 0;
   if (prev)
      for (; n < o->valuen; n++)
         if (o->values[n].isobj && o->values[n].obj == prev)
         {
            n++;
            break;
         }
   for (; n < o->valuen; n++)
      if (o->values[n].isobj && !strcmp (o->values[n].obj->tag, tag))
         return o->values[n].obj;
   return NULL;
}

void
load_pcb (void)
{
   int f = open (pcbfile, O_RDONLY);
   if (f < 0)
      err (1, "Cannot open %s", pcbfile);
   struct stat s;
   if (fstat (f, &s))
      err (1, "Cannot stat %s", pcbfile);
   char *data = mmap (NULL, s.st_size, PROT_READ, MAP_PRIVATE, f, 0);
   if (!data)
      errx (1, "Cannot access %s", pcbfile);
   const char *p = data;
   pcb = parse_obj (&p, data + s.st_size);
   munmap (data, s.st_size);
   close (f);
}

void
copy_file (FILE * o, const char *fn)
{
   int f = open (fn, O_RDONLY);
   if (f < 0)
      err (1, "Cannot open %s", fn);
   struct stat s;
   if (fstat (f, &s))
      err (1, "Cannot stat %s", fn);
   char *data = mmap (NULL, s.st_size, PROT_READ, MAP_PRIVATE, f, 0);
   if (!data)
      errx (1, "Cannot access %s", fn);
   fwrite (data, s.st_size, 1, o);
   munmap (data, s.st_size);
   close (f);
}

void
write_scad (void)
{
   obj_t *o,
    *o2,
    *o3;
   /* making scad file */
   FILE *f = stdout;
   if (strcmp (scadfile, "-"))
      f = fopen (scadfile, "w");
   if (!f)
      err (1, "Cannot open %s", scadfile);

   if (chdir (modeldir))
      errx (1, "Cannot access model dir %s", modeldir);

   if (strcmp (pcb->tag, "kicad_pcb"))
      errx (1, "Not a kicad_pcb (%s)", pcb->tag);
   obj_t *general = find_obj (pcb, "general", NULL);
   if (general)
   {
      if ((o = find_obj (general, "thickness", NULL)) && o->valuen == 1 && o->values[0].isnum)
         pcbthickness = o->values[0].num;
   }
   fprintf (f, "// Generated case design for %s\n", pcbfile);
   fprintf (f, "// By https://github.com/revk/PCBCase\n");
   {
      struct tm t;
      time_t now = time (0);
      localtime_r (&now, &t);
      fprintf (f, "// Generated %04d-%02d-%02d %02d:%02d:%02d\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
               t.tm_sec);
   }
   if ((o = find_obj (pcb, "title_block", NULL)))
      for (int n = 0; n < o->valuen; n++)
         if (o->values[n].isobj && (o2 = o->values[n].obj)->valuen >= 1)
         {
            if (o2->values[o2->valuen - 1].istxt)
               fprintf (f, "// %s:\t%s\n", o2->tag, o2->values[o2->valuen - 1].txt);
            else if (o2->values[0].isnum)
               fprintf (f, "// %s:\t%lf\n", o2->tag, o2->values[0].num);
         }
   fprintf (f, "//\n\n");
   fprintf (f, "// Globals\n");
   fprintf (f, "margin=%lf;\n", margin);
   fprintf (f, "overlap=%lf;\n", overlap);
   fprintf (f, "lip=%lf;\n", lip);
   fprintf (f, "casebase=%lf;\n", casebase);
   fprintf (f, "casetop=%lf;\n", casetop);
   fprintf (f, "casewall=%lf;\n", casewall);
   fprintf (f, "fit=%lf;\n", fit);
   fprintf (f, "edge=%lf;\n", edge);
   fprintf (f, "pcbthickness=%lf;\n", pcbthickness);
   fprintf (f, "nohull=%s;\n", nohull ? "true" : "false");
   fprintf (f, "hullcap=%lf;\n", hullcap);
   fprintf (f, "hulledge=%lf;\n", hulledge);
   fprintf (f, "useredge=%s;\n", layercase ? "true" : "false");

   double lx = DBL_MAX,
      hx = -DBL_MAX,
      ly = DBL_MAX,
      hy = -DBL_MAX;
   double ry;                   /* reference for Y, as it is flipped! */
   /* sanity */
   if (!pcbthickness)
      errx (1, "Specify pcb thickness");
   if (!lip)
      lip = pcbthickness / 2;
   void outline (const char *layer, const char *tag)
   {
      struct
      {
         double x1,
           y1;
         double xm,
           ym;
         double x2,
           y2;
         unsigned char arc:1;
         unsigned char used:1;
      } *cuts = NULL;
      int cutn = 0;

      void add (obj_t * o, double dx, double dy, double a)
      {
         void makecuts (double x1, double y1, double xm, double ym, double x2, double y2, int arc)
         {
            void translate (double *xp, double *yp)
            {
               if (a)
                  warnx ("TODO rotate footprint");
               (*xp) += dx;
               (*yp) += dy;
            }
            translate (&x1, &y1);
            translate (&x2, &y2);
            translate (&xm, &ym);
            void edges (double x, double y)
            {                   // Record limits
               if (x < lx)
                  lx = x;
               if (x > hx)
                  hx = x;
               if (y < ly)
                  ly = y;
               if (y > hy)
                  hy = y;
            }
            cuts = realloc (cuts, (cutn + 1) * sizeof (*cuts));
            if (!cuts)
               errx (1, "malloc");
            cuts[cutn].used = 0;
            cuts[cutn].x1 = x1;
            cuts[cutn].y1 = y1;
            edges (x1, y1);
            cuts[cutn].x2 = x2;
            cuts[cutn].y2 = y2;
            edges (x1, y1);
            if (arc)
            {
               cuts[cutn].xm = xm;
               cuts[cutn].ym = ym;
               edges (xm, ym);
            }
            cuts[cutn].arc = arc;
            cutn++;
         }
         obj_t *o2 = find_obj (o, "layer", NULL);
         if (!o2 || o2->valuen != 1 || !o2->values[0].istxt || strcmp (o2->values[0].txt, layer))
            return;
         if (!(o2 = find_obj (o, "end", NULL)) || !o2->values[0].isnum || !o2->values[1].isnum)
            return;
         double x2 = o2->values[0].num,
            y2 = o2->values[1].num;
         if (!(o2 = find_obj (o, "start", NULL)) || !o2->values[0].isnum || !o2->values[1].isnum)
         {
            if (!(o2 = find_obj (o, "center", NULL)) || o2->valuen != 2 || !o2->values[0].isnum || !o2->values[1].isnum)
               return;          /* not a circle */
            long double cx = o2->values[0].num,
               cy = o2->values[1].num;
            long double r = sqrtl ((cx - x2) * (cx - x2) + (cy - y2) * (cy - y2));
            makecuts (cx - r, cy, cx, cy + r, cx + r, cy, 1);
            makecuts (cx + r, cy, cx, cy - r, cx - r, cy, 1);
            return;
         }
         double x1 = o2->values[0].num,
            y1 = o2->values[1].num;
         double xm = 0,
            ym = 0;
         char arc = 0;
         if ((o2 = find_obj (o, "mid", NULL)) && o2->values[0].isnum && o2->values[1].isnum)
         {
            arc = 1;
            xm = o2->values[0].num;
            ym = o2->values[1].num;
         }
         makecuts (x1, y1, xm, ym, x2, y2, arc);
      }
      o = NULL;
      while ((o = find_obj (pcb, "gr_line", o)))
         add (o, 0, 0, 0);
      while ((o = find_obj (pcb, "gr_arc", o)))
         add (o, 0, 0, 0);
      while ((o = find_obj (pcb, "gr_circle", o)))
         add (o, 0, 0, 0);
      obj_t *fp = NULL;
      while ((fp = find_obj (pcb, "footprint", fp)))
      {
         o2 = find_obj (fp, "at", NULL);
         if (!o2 || o2->valuen < 2 || !o2->values[0].isnum || !o2->values[1].isnum)
            continue;
         long double x = o2->values[0].num,
            y = o2->values[1].num,
            a = 0;
         if (o2->valuen >= 3 && o2->values[2].isnum)
            a = o2->values[2].num;
         while ((o = find_obj (fp, "fp_line", o)))
            add (o, x, y, a);
         while ((o = find_obj (fp, "fp_arc", o)))
            add (o, x, y, a);
         while ((o = find_obj (fp, "fp_circle", o)))
            add (o, x, y, a);
      }
      ry = hy;
      char *points = NULL;
      size_t lpo;
      FILE *po = open_memstream (&points, &lpo);
      char *paths = NULL;
      size_t lpa;
      FILE *pa = open_memstream (&paths, &lpa);
      char started = 0;
      double *pointx = NULL;
      double *pointy = NULL;
      int pointn = 0,
         pointa = 0;
      int addpoint (double x, double y)
      {
         int p;
         for (p = 0; p < pointn && (pointx[p] != x || pointy[p] != y); p++);
         if (p == pointn)
         {
            if (p == pointa)
            {
               pointa += 100;
               pointx = realloc (pointx, sizeof (*pointx) * pointa);
               pointy = realloc (pointy, sizeof (*pointy) * pointa);
            }
            pointx[p] = x;
            pointy[p] = y;
            fprintf (po, "[%lf,%lf],", x, y);
            pointn++;
         }
         return p;
      }
      if (cutn)
      {                         /* Edge cut */
         double x = NAN,
            y = NAN;
         int start = -1;
         int todo = cutn;
         while (todo--)
         {
            int n,
              b1 = -1,
               b2 = -1;
            double d1 = 0,
               d2 = 0,
               t = 0,
               x1 = 0,
               y1 = 0,
               x2 = 0,
               y2 = 0;
            inline double dist (double x1, double y1)
            {
               return (x - x1) * (x - x1) + (y - y1) * (y - y1);
            }
            for (n = 0; n < cutn; n++)
               if (!cuts[n].used && ((t = dist (cuts[n].x1, cuts[n].y1)) < d1 || b1 < 0))
               {
                  b1 = n;
                  d1 = t;
               }
            for (n = 0; n < cutn; n++)
               if (!cuts[n].used && ((t = dist (cuts[n].x2, cuts[n].y2)) < d2 || b2 < 0))
               {
                  b2 = n;
                  d2 = t;
               }
            int b = 0;
            if (d1 < d2)
            {
               b = b1;
               x1 = cuts[b].x1;
               y1 = cuts[b].y1;
               x2 = cuts[b].x2;
               y2 = cuts[b].y2;
            } else
            {
               b = b2;
               x1 = cuts[b].x2;
               y1 = cuts[b].y2;
               x2 = cuts[b].x1;
               y2 = cuts[b].y1;
            }
            cuts[b].used = 1;
            if (!started || x1 != x || y1 != y)
            {
               if (start >= 0)
                  warnx ("Not closed path (%lf,%lf)", x1, y1);
               start = addpoint ((x = x1) - lx, ry - (y = y1));
               if (started)
                  fprintf (pa, "],");
               fprintf (pa, "[%d", start);
            }
            if (cuts[b].arc)
            {
               //warnx("Arc");
               //warnx("x1=%lf y1=%lf xm=%lf ym=%lf x2=%lf y2=%lf", x1, y1, xm, ym, x2, y2);
               double xm = cuts[b].xm;
               double ym = cuts[b].ym;
               double xq = (x1 + x2) / 2;
               double yq = (y1 + y2) / 2;
               double qm = sqrt ((xq - xm) * (xq - xm) + (yq - ym) * (yq - ym));
               double q2 = sqrt ((xq - x2) * (xq - x2) + (yq - y2) * (yq - y2));
               double as = atan2 (q2, qm);
               double r = q2 / cos (2 * as - M_PI / 2);
               //warnx("xq=%lf yq=%lf qm=%lf q2=%lf as=%lf r=%lf", xq, yq, qm, q2, as * 180.0 / M_PI, r);
               double cx = xm + (xq - xm) * r / qm;
               double cy = ym + (yq - ym) * r / qm;
               double a1 = atan2 (y1 - cy, x1 - cx);
               double am = atan2 (ym - cy, xm - cx);
               double a2 = atan2 (y2 - cy, x2 - cx);
               //warnx("cx=%lf cy=%lf a1=%lf am=%lf a2=%lf", cx, cy, a1 * 180.0 / M_PI, am * 180.0 / M_PI, a2 * 180.0 / M_PI);
               if (a2 <= a1 && (am > a1 || am < a2))
                  a2 += 2 * M_PI;
               else if (a2 >= a1 && (am < a1 || am > a2))
                  a2 -= 2 * M_PI;
               if (delta < r)
               {
                  double da = 2 * acos (1 - delta / r);
                  int steps = ((a2 - a1) / da + 1);
                  //warnx("da=%lf steps=%d", da * 180.0 / M_PI, steps);
                  if (steps < 0)
                     steps = -steps;
                  for (int i = 1; i < steps; i++)
                  {
                     double a = a1 + (a2 - a1) * i / steps;
                     int p = addpoint ((x = (cx + r * cos (a))) - lx, ry - (y = (cy + r * sin (a))));
                     if (p == start)
                        start = -1;
                     else
                        fprintf (pa, ",%d", p);
                  }
               }
            }
            started = 1;
            if (x2 != x || y2 != y)
            {
               int p = addpoint ((x = x2) - lx, ry - (y = y2));
               if (p == start)
                  start = -1;
               else
                  fprintf (pa, ",%d", p);
            }
         }
         if (started)
            fprintf (pa, "]");
      }
      fclose (po);
      if (lpo)
         points[--lpo] = 0;
      fclose (pa);
      if (tag)
         fprintf (f, "\nmodule %s(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[%s],paths=[%s]);}\n", tag,
                  points, paths);
      free (points);
      free (paths);
      free (pointx);
      free (pointy);
      free (cuts);
   }
   {
      char edgecuts[] = "Edge.Cuts";
      if (layerpcb > 0 && layerpcb < 10)
         sprintf (edgecuts, "User.%d", layerpcb);
      char casework[] = "Edge.Cuts";
      if (layercase > 0 && layercase < 01)
         sprintf (casework, "User.%d", layercase);
      else
         strcpy (casework, edgecuts);
      outline (edgecuts, NULL); // Gets min/max set for this - does not output
      outline (casework, "outline");    // Updates min/max before output
      outline (edgecuts, "pcb");        // Actually output this time
   }

   double edgewidth = 0,
      edgelength = 0;
   if (lx < DBL_MAX)
      edgewidth = hx - lx;
   if (ly < DBL_MAX)
      edgelength = hy - ly;
   if (!spacing)
      spacing = edgewidth + casewall * 2 + 10;
   if (!edgewidth || !edgelength)
      errx (1, "Specify pcb size");
   fprintf (f, "spacing=%lf;\n", spacing);
   fprintf (f, "pcbwidth=%lf;\n", edgewidth);
   fprintf (f, "pcblength=%lf;\n", edgelength);

   struct
   {
      char *filename;           // Filename (malloc)
      char *desc;               // Description (malloc)
      unsigned char ok:1;       // If file exists
      unsigned char n:1;        // If module expects n parameter
   } *modules = NULL;
   int modulen = 0;

   int find_module (char **fnp, const char *a, const char *b)
   {                            // Find a module by filename, and return number, <0 for failed (including file not existing)
      char *fn = *fnp;
      *fnp = NULL;
      int n;
      for (n = 0; n < modulen; n++)
         if (!strcmp (modules[n].filename, fn))
            break;
      if (n < modulen)
      {
         if (!modules[n].ok)
            return -1;
         return n;
      }
      modules = realloc (modules, (++modulen) * sizeof (*modules));
      if (!modules)
         errx (1, "malloc");
      memset (modules + n, 0, sizeof (*modules));
      modules[n].filename = fn;
      if (asprintf (&modules[n].desc, "%s %s", a, b) < 0)
         errx (1, "malloc");
      if (access (modules[n].filename, R_OK))
      {
         if (debug)
            warnx ("Not found %s %s %s", fn, a, b);
         return -1;
      }
      modules[n].ok = 1;
      if (debug)
         warnx ("New module %s %s %s", fn, a, b);
      return n;
   }

   const char *checkignore (const char *ref)
   {
      if (!ignore || !ref || !*ref || !*ignore)
         return NULL;
      const char *i = ignore;
      while (*i)
      {
         const char *r = ref;
         while (*i && *i != ',' && *r && *i == *r)
         {
            i++;
            r++;
         }
         if ((!*i || *i == ',') && !*r)
            return ref;
         while (*i && *i != ',')
            i++;
         while (*i == ',')
            i++;
      }
      return NULL;
   }

   /* The main PCB */
   fprintf (f, "// Populated PCB\nmodule board(pushed=false,hulled=false){\n");
   o = NULL;
   while ((o = find_obj (pcb, "footprint", o)))
   {
      char back = 0;            /* back of board */
      if (!(o2 = find_obj (o, "layer", NULL)) || o2->valuen != 1 || !o2->values[0].istxt)
         continue;
      if (!strcmp (o2->values[0].txt, "B.Cu"))
         back = 1;
      else if (strcmp (o2->values[0].txt, "F.Cu"))
         continue;
      const char *ref = NULL;
      o2 = NULL;
      while ((o2 = find_obj (o, "fp_text", o2)))
      {
         if (o2->valuen >= 2 && o2->values[1].istxt)
         {
            ref = o2->values[1].txt;
            break;
         }
      }
      if (checkignore (ref))
         continue;
      o2 = NULL;

      // Footprint level
      if (o->valuen >= 1 && o->values[0].istxt)
      {
         const char *r = strchr (o->values[0].txt, ':');
         if (r)
            r++;
         else
            r = o->values[0].txt;
         char *fn;
         if (asprintf (&fn, "%s.scad", r) < 0)
            errx (1, "malloc");
         int n = find_module (&fn, o->values[0].txt ? : ref ? : "-", r);
         int index = 0;
         if (n < 0)
         {                      // Consider parameterised maybe?
            const char *p = r;
            // try to find a likely number...
            for (p = r; *p && (*p != 'x' || !isdigit (p[1])); p++);     // Look for xN
            if (!*p)
               for (p = r; *p && (*p != '-' || !isdigit (p[1])); p++);  // Look for -N
            if (!*p)
               for (p = r; *p && (*p != '_' || !isdigit (p[1])); p++);  // Look for _N
            if (*p)
            {
               p++;
               int pos = (p - r);
               while (isdigit (*p))
                  index = index * 10 + (*p++) - '0';
               if (index)
               {                // Make filename
                  if (asprintf (&fn, "%s.scad", r) < 0)
                     errx (1, "malloc");
                  char *I = fn,
                     *O = fn;
                  while (*I && I < fn + pos)
                     *O++ = *I++;
                  *O++ = '0';
                  while (*I && isdigit (*I))
                     I++;
                  while (*I)
                     *O++ = *I++;
                  *O = 0;
                  n = find_module (&fn, o->values[0].txt ? : ref ? : "-", r);
                  if (n >= 0)
                     modules[n].n = 1;
               }
            }
         }
         if (n >= 0)
         {                      // footprint level 3D
            if (debug && ref)
               warnx ("Module %s %s%s", ref, ref, back ? " (back)" : "");
            if ((o3 = find_obj (o, "at", NULL)) && o3->valuen >= 2 && o3->values[0].isnum && o3->values[1].isnum)
            {
               fprintf (f, "translate([%lf,%lf,%lf])", o3->values[0].num - lx, ry - o3->values[1].num, back ? 0 : pcbthickness);
               if (o3->valuen >= 3 && o3->values[2].isnum)
                  fprintf (f, "rotate([0,0,%lf])", o3->values[2].num);
            }
            if (back)
               fprintf (f, "rotate([180,0,0])");
            if (modules[n].n && index)
               fprintf (f, "m%d(pushed,hulled,%d); // %s%s\n", n, index, modules[n].desc, back ? "" : " (back)");
            else
               fprintf (f, "m%d(pushed,hulled); // %s%s\n", n, modules[n].desc, back ? "" : " (back)");
            continue;
         }
      }

      int id = 0;
      while ((o2 = find_obj (o, "model", o2)))
      {
         if (o2->valuen < 1 || !o2->values[0].istxt)
            continue;           /* Not 3D model */
         id++;
         char *model = strdup (o2->values[0].txt);
         if (!model)
            errx (1, "malloc");
         char *leaf = strrchr (model, '/');
         if (leaf)
            leaf++;
         else
            leaf = model;
         char *e = strrchr (model, '.');
         if (e)
            *e = 0;
         char *fn;
         if (asprintf (&fn, "%s.scad", leaf) < 0)
            errx (1, "malloc");
         int n = find_module (&fn, o->values[0].txt ? : ref ? : "-", leaf);
         char *refn;
         if (asprintf (&refn, "%s.%d", ref, id) < 0)
            errx (1, "malloc");
         if (checkignore (refn))
         {
            free (refn);
            continue;
         }
         if (debug && ref)
            warnx ("Module %s.%d %s%s", ref, id, leaf, back ? " (back)" : "");
         free (refn);
         if (n >= 0)
         {
            if ((o3 = find_obj (o, "at", NULL)) && o3->valuen >= 2 && o3->values[0].isnum && o3->values[1].isnum)
            {
               fprintf (f, "translate([%lf,%lf,%lf])", o3->values[0].num - lx, ry - o3->values[1].num, back ? 0 : pcbthickness);
               if (o3->valuen >= 3 && o3->values[2].isnum)
                  fprintf (f, "rotate([0,0,%lf])", o3->values[2].num);
            }
            if (back)
               fprintf (f, "rotate([180,0,0])");
            if ((o3 = find_obj (o2, "offset", NULL)) && (o3 = find_obj (o3, "xyz", NULL)) && o3->valuen >= 3 && o3->values[0].isnum
                && o3->values[1].isnum && o3->values[2].isnum && (o3->values[0].num || o3->values[1].num || o3->values[2].num))
               fprintf (f, "translate([%lf,%lf,%lf])", o3->values[0].num, o3->values[1].num, o3->values[2].num);
            if ((o3 = find_obj (o2, "scale", NULL)) && (o3 = find_obj (o3, "xyz", NULL)) && o3->valuen >= 3 && o3->values[0].isnum
                && o3->values[1].isnum && o3->values[2].isnum && (o3->values[0].num != 1 || o3->values[1].num != 1
                                                                  || o3->values[2].num != 1))
               fprintf (f, "scale([%lf,%lf,%lf])", o3->values[0].num, o3->values[1].num, o3->values[2].num);
            if ((o3 = find_obj (o2, "rotate", NULL)) && (o3 = find_obj (o3, "xyz", NULL)) && o3->valuen >= 3 && o3->values[0].isnum
                && o3->values[1].isnum && o3->values[2].isnum && (o3->values[0].num || o3->values[1].num || o3->values[2].num))
               fprintf (f, "rotate([%lf,%lf,%lf])", -o3->values[0].num, -o3->values[1].num, -o3->values[2].num);
            fprintf (f, "m%d(pushed,hulled); // %s%s\n", n, modules[n].desc, back ? "" : " (back)");
         } else
         {
            fprintf (f, "// Missing %s.%d %s%s\n", ref, id, leaf, back ? " (back)" : "");
            warnx ("Missing %s.%d %s%s", ref, id, leaf, back ? " (back)" : "");
         }
         free (model);
      }
   }
   fprintf (f, "}\n\n");

   fprintf (f, "module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}\n");

   /* Used models */
   for (int n = 0; n < modulen; n++)
      if (modules[n].ok)
      {
         if (modules[n].n)
            fprintf (f, "module m%d(pushed=false,hulled=false,n=0)\n{ // %s\n", n, modules[n].desc);
         else
            fprintf (f, "module m%d(pushed=false,hulled=false)\n{ // %s\n", n, modules[n].desc);
         copy_file (f, modules[n].filename);
         fprintf (f, "}\n\n");
      }
   /* Final SCAD */
   copy_file (f, "final.scad");

   if (debug)
      fprintf (f, "board();\n#pcb();\n");
   else if (!norender)
      fprintf (f, "base(); translate([spacing,0,0])top();\n");

   if (f != stdout)
      fclose (f);
}

int
main (int argc, const char *argv[])
{
   poptContext optCon;          /* context for parsing  command - line options */
   {
      const struct poptOption optionsTable[] = {
         {"pcb-file", 'i', POPT_ARG_STRING, &pcbfile, 0, "PCB file", "filename"},
         {"scad-file", 'o', POPT_ARG_STRING, &scadfile, 0, "Openscad file", "filename"},
         {"ignore", 'I', POPT_ARG_STRING, &ignore, 0, "Ignore", "ref{,ref}"},
         {"base", 'b', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &casebase, 0, "Case base", "mm"},
         {"top", 't', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &casetop, 0, "Case top", "mm"},
         {"wall", 'w', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &casewall, 0, "Case wall", "mm"},
         {"edge", 'e', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &edge, 0, "Case edge", "mm"},
         {"fit", 'f', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &fit, 0, "Case fit", "mm"},
         {"hull-cap", 3, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &hullcap, 0, "Hull cap", "mm"},
         {"hull-edge", 3, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &hulledge, 0, "Hull edge", "mm"},
         {"no-hull", 'h', POPT_ARG_NONE, &nohull, 0, "No hull on parts"},
         {"margin", 'm', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &margin, 0, "margin", "mm"},
         {"overlap", 'O', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &overlap, 0, "overlap", "mm"},
         {"lip", 0, POPT_ARG_DOUBLE, &lip, 0, "lip offset (default pcbthickness/2)", "mm"},
         {"pcb", 0, POPT_ARG_INT, &layerpcb, 0, "Use User.N as PCB border instead of Edge.Cuts", "N"},
         {"case", 0, POPT_ARG_INT, &layercase, 0, "Use User.N as case border instead of pcb", "N"},
         {"pcb-thickness", 'T', POPT_ARG_DOUBLE, &pcbthickness, 0, "PCB thickness (default: auto)", "mm"},
         {"model-dir", 'M', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &modeldir, 0, "Model directory", "dir"},
         {"spacing", 's', POPT_ARG_DOUBLE, &spacing, 0, "Spacing (default: auto)", "mm"},
         {"curve-delta", 'D', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &delta, 0, "Curve delta", "mm"},
         {"no-render", 'n', POPT_ARG_NONE, &norender, 0, "No-render, just define base() and top()"},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
         POPT_AUTOHELP {}
      };

      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      /* poptSetOtherOptionHelp(optCon, ""); */

      int c;
      if ((c = poptGetNextOpt (optCon)) < -1)
         errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

      if (poptPeekArg (optCon) && !pcbfile)
         pcbfile = poptGetArg (optCon);

      if (poptPeekArg (optCon) || !pcbfile)
      {
         poptPrintUsage (optCon, stderr, 0);
         return -1;
      }
   }
   if (!scadfile)
   {
      const char *f = strrchr (pcbfile, '/');
      if (f)
         f++;
      else
         f = pcbfile;
      const char *e = strrchr (f, '.');
      if (!e || !strcmp (e, ".scad"))
         e = f + strlen (f);
      if (asprintf (&scadfile, "%.*s.scad", (int) (e - pcbfile), pcbfile) < 0)
         errx (1, "malloc");
   }
   load_pcb ();
   write_scad ();

   poptFreeContext (optCon);
   return 0;
}
