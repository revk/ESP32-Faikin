// Daikin A/C log to mariadb from MQTT
// Copyright (c) 2022 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <err.h>
#include <malloc.h>
#include <time.h>
#include <sqllib.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <ajl.h>

int main(int argc, const char *argv[])
{
   const char *sqlhostname = NULL;
   const char *sqldatabase = "env";
   const char *sqlusername = NULL;
   const char *sqlpassword = NULL;
   const char *sqlconffile = NULL;
   const char *sqltable = "daikin";
   const char *mqtthostname = "localhost";
   const char *mqttusername = NULL;
   const char *mqttpassword = NULL;
   const char *mqttprefix = "Daikin";
   const char *mqttid = NULL;
   int interval = 60;
   int debug = 0;
   {                            // POPT
      poptContext optCon;       // context for parsing command-line options
      const struct poptOption optionsTable[] = {
         { "sql-conffile", 'c', POPT_ARG_STRING, &sqlconffile, 0, "SQL conf file", "filename" },
         { "sql-hostname", 'H', POPT_ARG_STRING, &sqlhostname, 0, "SQL hostname", "hostname" },
         { "sql-database", 'd', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &sqldatabase, 0, "SQL database", "db" },
         { "sql-username", 'U', POPT_ARG_STRING, &sqlusername, 0, "SQL username", "name" },
         { "sql-password", 'P', POPT_ARG_STRING, &sqlpassword, 0, "SQL password", "pass" },
         { "sql-table", 't', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &sqltable, 0, "SQL table", "table" },
         { "sql-debug", 'v', POPT_ARG_NONE, &sqldebug, 0, "SQL Debug" },
         { "mqtt-hostname", 'h', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &mqtthostname, 0, "MQTT hostname", "hostname" },
         { "mqtt-username", 'u', POPT_ARG_STRING, &mqttusername, 0, "MQTT username", "username" },
         { "mqtt-password", 'p', POPT_ARG_STRING, &mqttpassword, 0, "MQTT password", "password" },
         { "mqtt-prefix", 'a', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &mqttprefix, 0, "MQTT prefix", "prefix" },
         { "mqtt-id", 0, POPT_ARG_STRING, &mqttid, 0, "MQTT id", "id" },
         { "interval", 'i', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &interval, 0, "Recording interval", "seconds" },
         { "debug", 'V', POPT_ARG_NONE, &debug, 0, "Debug" },
         POPT_AUTOHELP { }
      };

      optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);

      int c;
      if ((c = poptGetNextOpt(optCon)) < -1)
         errx(1, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));

      if (poptPeekArg(optCon))
      {
         poptPrintUsage(optCon, stderr, 0);
         return -1;
      }
      poptFreeContext(optCon);
   }
   SQL sql;
   int e = mosquitto_lib_init();
   if (e)
      errx(1, "MQTT init failed %s", mosquitto_strerror(e));
   struct mosquitto *mqtt = mosquitto_new(mqttid, 1, NULL);
   if (mqttusername)
   {
      e = mosquitto_username_pw_set(mqtt, mqttusername, mqttpassword);
      if (e)
         errx(1, "MQTT auth failed %s", mosquitto_strerror(e));
   }
   void connect(struct mosquitto *mqtt, void *obj, int rc) {
      obj = obj;
      rc = rc;
      char *sub = NULL;
      asprintf(&sub, "%s/#", mqttprefix);
      int e = mosquitto_subscribe(mqtt, NULL, sub, 0);
      if (e)
         errx(1, "MQTT subscribe failed %s (%s)", mosquitto_strerror(e), sub);
      if (debug)
         warnx("MQTT Sub %s", sub);
      free(sub);
   }
   void disconnect(struct mosquitto *mqtt, void *obj, int rc) {
      obj = obj;
      rc = rc;
   }
   SQL_RES *res = NULL;
   void message(struct mosquitto *mqtt, void *obj, const struct mosquitto_message *msg) {
      obj = obj;
      char *topic = strdupa(msg->topic);
      if (!msg->payloadlen)
      {
         warnx("No payload %s", topic);
         return;
      }
      char *tag = strrchr(topic, '/');
      if (!tag)
      {
         warnx("Unknown topic %s", topic);
         return;
      }
      *tag++ = 0;
      j_t data = j_create();
      const char *e = j_read_mem(data, msg->payload, msg->payloadlen);
      if (e)
         warnx("Bad JSON [%s] Val [%.*s]", tag, msg->payloadlen, (char *) msg->payload);
      else
      {                         // Process log
         if (debug)
            warnx("%.*s", msg->payloadlen, (char *) msg->payload);
         if (!res)
            res = sql_safe_query_store_free(&sql, sql_printf("SELECT * FROM `%#S` LIMIT 0", sqltable));
         sql_string_t s = { };
         sql_sprintf(&s, "INSERT IGNORE INTO `%#S` SET `tag`=%#s,`when`=NOW()", sqltable, tag);
         int changed = 0;
         j_t j;
         j_t find(const char *name, const char *type) {
            j_t j = j_find(data, name);
            if (!j)
               return j;
            void check(const char *prefix, const char *type) {
               char field[100];
               sprintf(field, "%s%s", prefix, name);
               if (sql_colnum(res, field) < 0)
               {
                  sql_safe_query_free(&sql, sql_printf("ALTER TABLE `%#S` ADD `%#S` %s", sqltable, field, type));
                  changed++;
               }
            }
            if (*type == '~' || *type == '=')
            {
               check("min", type + 1);
               check("max", type + 1);
               if (*type == '~')
                  type++;
               else
                  type = NULL;
            }
            if (type)
               check("", type);
            return j;
         }
#define	b(name)	if((j=find(#name,"decimal(4,2)"))){sql_sprintf(&s,",`%#S`=%s",#name,j_istrue(j)?"1":j_isbool(j)?"0":j_isnumber(j)?j_val(j):"NULL");}
#define	i(name)	if((j=find(#name,"~int"))){if(j_isarray(j)&&j_len(j)==3&&j_isnumber(j_index(j,0))&&j_isnumber(j_index(j,1))&&j_isnumber(j_index(j,2)))	\
		sql_sprintf(&s,",`min%#S`=%s,`%#S`=%s,`max%#S`=%s",#name,j_val(j_index(j,0)),#name,j_val(j_index(j,1)),#name,j_val(j_index(j,2))); \
		else if(j_isnumber(j))sql_sprintf(&s,",`min%#S`=%s,`%#S`=%s,`max%#S`=%s",#name,j_val(j),#name,j_val(j),#name,j_val(j));}
#define	t(name)	if((j=find(#name,"~decimal(6,2)"))){if(j_isarray(j)&&j_len(j)==3&&j_isnumber(j_index(j,0))&&j_isnumber(j_index(j,1))&&j_isnumber(j_index(j,2)))	\
		sql_sprintf(&s,",`min%#S`=%s,`%#S`=%s,`max%#S`=%s",#name,j_val(j_index(j,0)),#name,j_val(j_index(j,1)),#name,j_val(j_index(j,2))); \
		else if(j_isnumber(j))sql_sprintf(&s,",`min%#S`=%s,`%#S`=%s,`max%#S`=%s",#name,j_val(j),#name,j_val(j),#name,j_val(j));}
#define	r(name)	if((j=find(#name,"=decimal(6,2)"))){if(j_isarray(j)&&j_len(j)==2&&j_isnumber(j_index(j,0))&&j_isnumber(j_index(j,1))) \
		sql_sprintf(&s,",`min%#S`=%s,`max%#S`=%s",#name,j_val(j_index(j,0)),#name,j_val(j_index(j,1)));	\
		else if(j_isnumber(j))sql_sprintf(&s,",`min%#S`=%s,`max%#S`=%s",#name,j_val(j),#name,j_val(j));}
#define e(name,t) if((j=find(#name,"char(1)"))){if(j_isstring(j))sql_sprintf(&s,",`%#S`=%#s",#name,j_val(j));}
#include "main/acextras.m"
         sql_safe_query_s(&sql, &s);
         if (changed)
         {
            sql_free_result(res);
            res = NULL;
         }
      }
   }

   mosquitto_connect_callback_set(mqtt, connect);
   mosquitto_disconnect_callback_set(mqtt, disconnect);
   mosquitto_message_callback_set(mqtt, message);
   e = mosquitto_connect(mqtt, mqtthostname, 1883, 60);
   if (e)
      errx(1, "MQTT connect failed (%s) %s", mqtthostname, mosquitto_strerror(e));
   sql_real_connect(&sql, sqlhostname, sqlusername, sqlpassword, sqldatabase, 0, NULL, 0, 1, sqlconffile);
   e = mosquitto_loop_forever(mqtt, -1, 1);
   if (e)
      errx(1, "MQTT loop failed %s", mosquitto_strerror(e));
   mosquitto_destroy(mqtt);
   mosquitto_lib_cleanup();
   sql_close(&sql);
   return 0;
}
