/***********************************************************
* This file is part of glyr
* + a commnadline tool and library to download various sort of musicrelated metadata.
* + Copyright (C) [2011]  [Christopher Pahl]
* + Hosted at: https://github.com/sahib/glyr
*
* glyr is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* glyr is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with glyr. If not, see <http://www.gnu.org/licenses/>.
**************************************************************/

#include "cache_intern.h"
#include "cache.h"
#include "core.h"
#include "glyr.h"
#include "register_plugins.h"

/* How long to wait till returning SQLITE_BUSY */
#define DB_BUSY_WAIT 5000

#define DO_PROFILE false

#if DO_PROFILE
    GTimer * select_callback_timer = NULL;
    float select_callback_spent = 0;
#endif


/* ------------------------------------------------------------------ */
static void create_table_defs(GlyrDatabase * db);
static void insert_cache_data(GlyrDatabase * db, GlyrQuery * query, GlyrMemCache * cache);
static void execute(GlyrDatabase * db, const gchar * sql_statement);
static GLYR_FIELD_REQUIREMENT get_req(GlyrQuery * q);
static int delete_callback(void * result, int argc, char ** argv, char ** azColName);
static gchar * convert_from_option_to_sql(GlyrQuery * q);
static double get_current_time(void);
static void add_to_cache_list(GlyrMemCache ** list, GlyrMemCache * to_add);
static int select_callback(void * result, int argc, char ** argv, char ** azColName);

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

typedef struct
{
    GlyrDatabase * con;
    gint deleted;
    gint max_delete;

} delete_callback_data;

/*--------------------------------------------------------------*/

typedef struct
{
    GlyrMemCache ** result;
    GlyrQuery * query;
    gint counter;
    glyr_foreach_callback cb;
    void * userptr;

} select_callback_data;

/*--------------------------------------------------------------*/

GlyrDatabase * glyr_db_init(char * root_path)
{
    if(sqlite3_threadsafe() == FALSE)
    {
        glyr_message(-1,NULL,"WARNING: Your SQLite version seems not to be threadsafe? \n"
                             "         Expect corrupted data and other weird behaviour!\n");
    }

#if DO_PROFILE
    select_callback_timer = g_timer_new();
#endif

    GlyrDatabase * to_return = NULL;
    if(root_path != NULL && g_file_test(root_path,G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS) == TRUE)
    {
        sqlite3 * db_connection = NULL;

        /* Use file:// Urls when supported */
#if SQLITE_VERSION_NUMBER >= 3007007
        gchar * db_file_path = g_strdup_printf("file://%s%s%s",root_path,(g_str_has_suffix(root_path,"/") ? "" : "/"),GLYR_DB_FILENAME);
        gint db_err = sqlite3_open_v2(db_file_path,&db_connection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI | SQLITE_OPEN_FULLMUTEX, NULL);
#else
        gchar * db_file_path = g_strdup_printf("%s%s%s",root_path,(g_str_has_suffix(root_path,"/") ? "" : "/"),GLYR_DB_FILENAME);
        gint db_err = sqlite3_open_v2(db_file_path,&db_connection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
#endif

        if(db_err == SQLITE_OK)
        {
            to_return = g_malloc0(sizeof(GlyrDatabase));
            to_return->root_path = g_strdup(root_path);
            to_return->db_handle = db_connection;
            sqlite3_busy_timeout(db_connection,DB_BUSY_WAIT);
            create_table_defs(to_return);
        }
        else
        {
            glyr_message(-1,NULL,"Connecting to database failed: %s\n",sqlite3_errmsg(db_connection));
            sqlite3_close(db_connection);
        }
        g_free(db_file_path);
    }
    return to_return;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/


void glyr_db_destroy(GlyrDatabase * db_object)
{
    if(db_object != NULL)
    {
        int db_err = sqlite3_close(db_object->db_handle);
        if(db_err == SQLITE_OK)
        {
            g_free((gchar*)db_object->root_path);
            g_free(db_object);
        }
        else
        {
            glyr_message(-1,NULL,"Disconnecting database failed: %s\n",sqlite3_errmsg(db_object->db_handle));
        }
    }
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

int glyr_db_edit(GlyrDatabase * db, GlyrQuery * query, GlyrMemCache * edited)
{
    int result = 0;
    if(db && query)
    {
        result = glyr_db_delete(db,query);
        if(result != 0)
        {
            for(GlyrMemCache * elem = edited; elem; elem = elem->next)
            {
                glyr_db_insert(db,query,edited);
            }
        }
    }
    return result;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void glyr_db_replace(GlyrDatabase * db, unsigned char * md5sum, GlyrQuery * query, GlyrMemCache * data)
{
    if(db != NULL && md5sum != NULL) 
    {
        gchar * sql = "DELETE FROM metadata WHERE data_checksum = ? ;\n";
        sqlite3_stmt *stmt = NULL;
        sqlite3_prepare_v2(db->db_handle, sql, strlen(sql) + 1, &stmt, NULL);
        sqlite3_bind_blob(stmt, 1, md5sum, 16, SQLITE_STATIC);

        if(sqlite3_step(stmt) != SQLITE_DONE) 
        {
            glyr_message(1,query,"Error message: %s\n", sqlite3_errmsg(db->db_handle));
        }

        sqlite3_finalize(stmt);

        if(data != NULL)
        {
            glyr_db_insert(db,query,data);
        }
    }
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

#define ADD_CONSTRAINT(TO_CONSTR, FIELDNAME, VARNAME)                    \
{                                                                        \
    gchar * lower = g_utf8_strdown(VARNAME,-1);                          \
    if(lower != NULL)                                                    \
    {                                                                    \
        TO_CONSTR = sqlite3_mprintf("AND %s = '%q'\n",FIELDNAME,lower);  \
        g_free(lower);                                                   \
    }                                                                    \
}


gint glyr_db_delete(GlyrDatabase * db, GlyrQuery * query)
{
    gint result = 0;
    if(db && query)
    {
        /* Find out which fields are required for this getter */
        GLYR_FIELD_REQUIREMENT reqs = get_req(query);

        /* Spaces in SQL statements just for pretty debug printing */
        gchar * artist_constr = "";
        if((reqs & GLYR_REQUIRES_ARTIST) != 0 && query->artist)
        {
            ADD_CONSTRAINT(artist_constr,"a.artist_name",query->artist);
        }

        gchar * album_constr  = "";
        if((reqs & GLYR_REQUIRES_ALBUM ) != 0 && query->album)
        {                  
            ADD_CONSTRAINT(album_constr,"b.album_name",query->album);
        }

        gchar * title_constr = "";
        if((reqs & GLYR_REQUIRES_TITLE ) != 0 && query->title)
        {
            ADD_CONSTRAINT(title_constr,"t.title_name",query->title);
        }

        /* Get a SQL formatted list of enabled providers: IN('lastfm','...') */
        gchar * from_argument_list = convert_from_option_to_sql(query);

        /* Check if links should be deleted */        
        gchar * img_url_constr = "";
        if(TYPE_IS_IMAGE(query->type))
        {
            if(query->download == FALSE)
            {
                img_url_constr = sqlite3_mprintf("AND data_type = %d ", GLYR_TYPE_IMG_URL);
            }
            else
            {
                img_url_constr = sqlite3_mprintf("AND NOT data_type = %d ", GLYR_TYPE_IMG_URL);
            }
        }

        gchar * sql = sqlite3_mprintf(
                "SELECT get_type,                                     \n"
                "       artist_id,                                    \n"
                "       album_id,                                     \n"
                "       title_id,                                     \n"
                "       provider_id                                   \n"
                "       FROM metadata AS m                            \n"
                "LEFT JOIN artists    AS a ON a.rowid = m.artist_id   \n"
                "LEFT JOIN albums     AS b ON b.rowid = m.album_id    \n"
                "LEFT JOIN titles     AS t ON t.rowid = m.title_id    \n"
                "INNER JOIN providers AS p ON p.rowid = m.provider_id \n"
                "WHERE                                                \n"
                "       m.get_type  = %d                              \n"
                "   %s  -- Title  Contraint                           \n"
                "   %s  -- Album  Constraint                          \n"
                "   %s  -- Artist Constraint                          \n"
                "   AND p.provider_name IN(%s)                        \n"
                "   %s  -- 'IsALink' Constraint                       \n"
                "LIMIT %d;                                            \n"
                ,                   /* ------------------------------ */
                query->type,        /* Limit to <type>                */
                title_constr,       /* Title Constr, may be empty     */ 
                album_constr,       /* Album Constr, may be empty     */ 
                artist_constr,      /* Artist Constr, may be empty    */
                from_argument_list, /* Provider contraint             */ 
                img_url_constr,     /* Search for links?              */
                query->number       /* LIMIT to <number>              */
                    );


        if(sql != NULL)
        {
            delete_callback_data cb_data;
            cb_data.con = db;
            cb_data.deleted = 0;
            cb_data.max_delete = query->number;

            gchar * err_msg = NULL;
            sqlite3_exec(db->db_handle,sql,delete_callback,&cb_data,&err_msg);
            if(err_msg != NULL)
            {
                glyr_message(-1,NULL,"SQL Delete error: %s\n",err_msg);
                sqlite3_free(err_msg);
            }
            sqlite3_free(sql);
            result = cb_data.deleted;
        }

        /**
         * Free ressources with according free calls,
         */

        if(*artist_constr)
            sqlite3_free(artist_constr);

        if(*album_constr)
            sqlite3_free(album_constr);

        if(*title_constr)
            sqlite3_free(title_constr);

        if(*img_url_constr)
            sqlite3_free(img_url_constr);

        g_free(from_argument_list);

    }
    return result;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/


void glyr_db_foreach(GlyrDatabase * db, glyr_foreach_callback cb, void * userptr)
{
    if(db != NULL && cb != NULL)
    {
        const gchar * select_all = 
            "SELECT artist_name,                                      \n"
            "        album_name,                                      \n"
            "        title_name,                                      \n"
            "        provider_name,                                   \n"
            "        source_url,                                      \n"
            "        image_type_name,                                 \n"
            "        track_duration,                                  \n"
            "        get_type,                                        \n"
            "        data_type,                                       \n"
            "        data_size,                                       \n"
            "        data_is_image,                                   \n"
            "        data_checksum,                                   \n"
            "        data,                                            \n"
            "        rating,                                          \n"
            "        timestamp                                        \n"
            "FROM metadata as m                                       \n"
            "LEFT JOIN artists     AS a ON m.artist_id     = a.rowid  \n"
            "LEFT JOIN albums      AS b ON m.album_id      = b.rowid  \n"
            "LEFT JOIN titles      AS t ON m.title_id      = t.rowid  \n"
            "LEFT JOIN image_types AS i ON m.image_type_id = i.rowid  \n"
            "JOIN providers AS p on m.provider_id          = p.rowid  \n"
            "ORDER BY rating,timestamp;                               \n";

        select_callback_data scb_data;
        scb_data.cb = cb;
        scb_data.userptr = userptr;

        GlyrQuery dummy;
        dummy.number = G_MAXINT;
        scb_data.query = &dummy;
        scb_data.counter = 0;
        scb_data.result = NULL;

        int rc = SQLITE_OK;
        char * err_msg = NULL;
        if((rc = sqlite3_exec(db->db_handle,select_all,select_callback,&scb_data,&err_msg)) != SQLITE_OK)
        {
            if(rc != SQLITE_ABORT)
            {
                glyr_message(-1,NULL,"SQL Foreach error: %s\n",err_msg);
            }
            sqlite3_free(err_msg);
        }
    }
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

GlyrMemCache * glyr_db_lookup(GlyrDatabase * db, GlyrQuery * query)
{
    GlyrMemCache * result = NULL;
    if(db != NULL && query != NULL)
    {
        GLYR_FIELD_REQUIREMENT reqs = get_req(query);
        gchar * artist_constr = "";
        if((reqs & GLYR_REQUIRES_ARTIST) != 0)
        {
            ADD_CONSTRAINT(artist_constr,"artist_name",query->artist);
        }
        gchar * album_constr  = "";
        if((reqs & GLYR_REQUIRES_ALBUM ) != 0)
        {
            ADD_CONSTRAINT(album_constr,"album_name",query->album);
        }

        gchar * title_constr = "";
        if((reqs & GLYR_REQUIRES_TITLE ) != 0)
        {
            ADD_CONSTRAINT(title_constr,"title_name",query->title);
        }

        gchar * from_argument_list = convert_from_option_to_sql(query);
        gchar * img_url_constr = "";

        if(TYPE_IS_IMAGE(query->type))
        {
            if(query->download == FALSE)
            {
                img_url_constr = sqlite3_mprintf("AND data_type = %d ", GLYR_TYPE_IMG_URL);
            }
            else
            {
                img_url_constr = sqlite3_mprintf("AND NOT data_type = %d ", GLYR_TYPE_IMG_URL);
            }
        }

        gchar * sql = sqlite3_mprintf(
                "SELECT artist_name,                                      \n"
                "        album_name,                                      \n"
                "        title_name,                                      \n"
                "        provider_name,                                   \n"
                "        source_url,                                      \n"
                "        image_type_name,                                 \n"
                "        track_duration,                                  \n"
                "        get_type,                                        \n"
                "        data_type,                                       \n"
                "        data_size,                                       \n"
                "        data_is_image,                                   \n"
                "        data_checksum,                                   \n"
                "        data,                                            \n"
                "        rating,                                          \n"
                "        timestamp                                        \n"
                "FROM metadata as m                                       \n"
                "LEFT JOIN artists AS a ON m.artist_id  = a.rowid         \n"
                "LEFT JOIN albums  AS b ON m.album_id   = b.rowid         \n"
                "LEFT JOIN titles  AS t ON m.title_id   = t.rowid         \n"
                "JOIN providers as p on m.provider_id   = p.rowid         \n"
                "LEFT JOIN image_types as i on m.image_type_id = i.rowid  \n"
                "WHERE m.get_type = %d                                    \n"
                "                   %s  -- Title constr.                  \n"
                "                   %s  -- Album constr.                  \n"
                "                   %s  -- Artist constr.                 \n"
                "                   %s                                    \n"
                "           AND provider_name IN(%s)                      \n"
//                "ORDER BY rating,timestamp                                \n" // GAAAAARGHL!!
                "LIMIT %d;                                                \n",
            query->type,
            title_constr,
            album_constr,
            artist_constr,
            img_url_constr,
            from_argument_list, 
            query->number
                );

        if(sql != NULL)
        {
            select_callback_data data;
            data.result = &result;
            data.query = query;
            data.counter = 0;
            data.cb = NULL;
            data.userptr = NULL;

            gchar * err_msg = NULL;
            sqlite3_exec(db->db_handle,sql,select_callback,&data,&err_msg);
            if(err_msg != NULL)
            {
                glyr_message(-1,NULL,"glyr_db_lookup: %s\n",err_msg);
                sqlite3_free(err_msg);
            }
            sqlite3_free(sql);
        }

#if DO_PROFILE
        g_message("Spent %.5f Seconds in Selectcallback.\n",select_callback_spent);
        select_callback_spent = 0;
#endif

        if(*artist_constr)
        {
            sqlite3_free(artist_constr);
        }
        if(*album_constr)
        {
            sqlite3_free(album_constr);
        }
        if(*title_constr)
        {
            sqlite3_free(title_constr);
        }

        g_free(from_argument_list);

        if(*img_url_constr != '\0')
        {
            sqlite3_free(img_url_constr);
        }
    }
    return result;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

#define INSERT_STRING(SQL,ARG) {                       \
    if(SQL && ARG) {                                   \
        gchar * lower_str = g_utf8_strdown(ARG,-1);    \
        gchar * sql = sqlite3_mprintf(SQL,lower_str);  \
        execute(db,sql);                               \
        sqlite3_free(sql);                             \
        g_free(lower_str);                             \
    }                                                  \
}

/* Ensure no invalid data comes in */
#define ABORT_ON_FAILED_REQS(ARG) {                                \
    if(ARG == NULL) {                                              \
        glyr_message(-1,NULL,"Warning: %s != NULL failed",#ARG);  \
        return;                                                   \
    }                                                              \
}

void glyr_db_insert(GlyrDatabase * db, GlyrQuery * q, GlyrMemCache * cache)
{
    if(db && q && cache)
    {
        GLYR_FIELD_REQUIREMENT reqs = get_req(q);
        execute(db,"BEGIN IMMEDIATE;");
        if((reqs & GLYR_REQUIRES_ARTIST) || (reqs & GLYR_OPTIONAL_ARTIST)) {
            ABORT_ON_FAILED_REQS(q->artist); 
            INSERT_STRING("INSERT OR IGNORE INTO artists VALUES('%q');",q->artist); 
        }
        if((reqs & GLYR_REQUIRES_ALBUM) || (reqs & GLYR_OPTIONAL_ALBUM)) {
            ABORT_ON_FAILED_REQS(q->album); 
            INSERT_STRING("INSERT OR IGNORE INTO albums  VALUES('%q');",q->album);
        }
        if((reqs & GLYR_REQUIRES_TITLE) || (reqs & GLYR_OPTIONAL_TITLE)) {
            ABORT_ON_FAILED_REQS(q->title); 
            INSERT_STRING("INSERT OR IGNORE INTO titles  VALUES('%q');",q->title);
        }

        gchar * provider = (cache->prov) ? cache->prov : "none"; 
        INSERT_STRING("INSERT OR IGNORE INTO providers VALUES('%q');",provider);
        insert_cache_data(db,q,cache);
        execute(db,"COMMIT;");
    }
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

// --------- INTERNALS ------------ //

static void execute(GlyrDatabase * db, const gchar * sql_statement)
{
    if(db && sql_statement)
    {
        char * err_msg = NULL;
        sqlite3_exec(db->db_handle,sql_statement,NULL,NULL,&err_msg);
        if(err_msg != NULL)
        {
            glyr_message(-1,NULL,"glyr_db_execute: SQL error: %s\n", err_msg);
            sqlite3_free(err_msg);
        }
    }
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/


static void create_table_defs(GlyrDatabase * db)
{
    execute(db,
            "PRAGMA synchronous = 1;                                                     \n"
            "PRAGMA quick_check;                                                         \n"
            "PRAGMA temp_store = 2;                                                      \n"
            "BEGIN IMMEDIATE;                                                            \n"
            "-- Provider                                                                 \n"
            "CREATE TABLE IF NOT EXISTS providers (provider_name VARCHAR(20) UNIQUE);    \n"
            "                                                                            \n"
            "-- Artist                                                                   \n"
            "CREATE TABLE IF NOT EXISTS artists (artist_name VARCHAR(128) UNIQUE);       \n"
            "CREATE TABLE IF NOT EXISTS albums  (album_name  VARCHAR(128) UNIQUE);       \n"
            "CREATE TABLE IF NOT EXISTS titles  (title_name  VARCHAR(128) UNIQUE);       \n"
            "                                                                            \n"
            "-- Enum                                                                     \n"
            "CREATE TABLE IF NOT EXISTS image_types(image_type_name VARCHAR(16) UNIQUE); \n"
            "CREATE TABLE IF NOT EXISTS db_version(version INTEGER UNIQUE);              \n"
            "                                                                            \n"
            "-- MetaData                                                                 \n"
            "CREATE TABLE IF NOT EXISTS metadata(                                        \n"
            "                     artist_id INTEGER,                                     \n"
            "                     album_id  INTEGER,                                     \n"
            "                     title_id  INTEGER,                                     \n"
            "                     provider_id INTEGER,                                   \n"
            "                     source_url  VARCHAR(512),                              \n"
            "                     image_type_id INTEGER,                                 \n"
            "                     track_duration INTEGER,                                \n"
            "                     get_type INTEGER,                                      \n"
            "                     data_type INTEGER,                                     \n"
            "                     data_size INTEGER,                                     \n"
            "                     data_is_image INTEGER,                                 \n"
            "                     data_checksum BLOB,                                    \n"
            "                     data BLOB,                                             \n"
            "                     rating INTEGER,                                        \n"
            "                     timestamp FLOAT                                        \n"
            ");                                                                          \n"
            "CREATE INDEX IF NOT EXISTS index_artist_id   ON metadata(artist_id);        \n"
            "CREATE INDEX IF NOT EXISTS index_album_id    ON metadata(album_id);         \n"
            "CREATE INDEX IF NOT EXISTS index_title_id    ON metadata(title_id);         \n"
            "CREATE INDEX IF NOT EXISTS index_provider_id ON metadata(provider_id);      \n"
            "CREATE UNIQUE INDEX IF NOT EXISTS index_unique                              \n"
            "       ON metadata(get_type,data_type,data_checksum,source_url);            \n"
            "-- Insert imageformats                                                      \n"
            "INSERT OR IGNORE INTO image_types VALUES('jpeg');                           \n"
            "INSERT OR IGNORE INTO image_types VALUES('jpg');                            \n"
            "INSERT OR IGNORE INTO image_types VALUES('png');                            \n"
            "INSERT OR IGNORE INTO image_types VALUES('gif');                            \n"
            "INSERT OR IGNORE INTO image_types VALUES('tiff');                           \n"
            "INSERT OR IGNORE INTO db_version VALUES(2);                                 \n"
            "COMMIT;                                                                     \n"
            );
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

/**
 *  Return the current time as double:
 *  <seconds>.<microseconds/one_second>
 */
static double get_current_time(void)
{
    struct timeval tim;
    gettimeofday(&tim, NULL);
    return tim.tv_sec + (tim.tv_usec/1000000.0);
}

/*--------------------------------------------------------------*/


static void insert_cache_data(GlyrDatabase * db, GlyrQuery * query, GlyrMemCache * cache)
{
    if(db && query && cache)
    {

        char * sql = sqlite3_mprintf(
                "INSERT OR IGNORE INTO metadata VALUES(                            \n"
                "  (SELECT rowid FROM artists   WHERE artist_name   = LOWER('%q')),\n"
                "  (SELECT rowid FROM albums    WHERE album_name    = LOWER('%q')),\n"
                "  (SELECT rowid FROM titles    WHERE title_name    = LOWER('%q')),\n"
                "  (SELECT rowid FROM providers WHERE provider_name = LOWER('%q')),\n"
                "  ?,                                                              \n"
                "  (SELECT rowid FROM image_types WHERE image_type_name = LOWER('%q')),\n"
                "  ?,?,?,?,?,?,?,?,?                                               \n"
                ");                                                                \n",
                query->artist,
                query->album,
                query->title,
                (cache->prov) ? cache->prov : "none",
                cache->img_format
                );

        sqlite3_stmt *stmt = NULL;
        sqlite3_prepare_v2(db->db_handle, sql, strlen(sql) + 1, &stmt, NULL);

        if(cache->dsrc != NULL)
            sqlite3_bind_text(stmt, 1, cache->dsrc,strlen(cache->dsrc) + 1, SQLITE_STATIC);
        else
            glyr_message(1,query,"glyr: Warning: Attempting to insert cache with missing source-url!\n");

        sqlite3_bind_int (stmt, 2, cache->duration);
        sqlite3_bind_int (stmt, 3, query->type);
        sqlite3_bind_int( stmt, 4, cache->type);
        sqlite3_bind_int( stmt, 5, cache->size);
        sqlite3_bind_int( stmt, 6, cache->is_image);
        sqlite3_bind_blob(stmt, 7, cache->md5sum, sizeof cache->md5sum, SQLITE_STATIC);

        if(cache->data != NULL)
            sqlite3_bind_blob(stmt, 8, cache->data, cache->size, SQLITE_STATIC);
        else 
            glyr_message(1,query,"glyr: Warning: Attempting to insert cache with missing data!\n");

        sqlite3_bind_int( stmt, 9, cache->rating);
        sqlite3_bind_double( stmt,10, get_current_time());

        if(sqlite3_step(stmt) != SQLITE_DONE) 
            glyr_message(1,query,"glyr_db_insert: SQL failure: %s\n", sqlite3_errmsg(db->db_handle));

        sqlite3_finalize(stmt);
        sqlite3_free(sql);
    }
}



/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/


static void add_to_cache_list(GlyrMemCache ** list, GlyrMemCache * to_add)
{
    if(to_add && list)
    {
        GlyrMemCache * head = *list;
        if(head == NULL)
        {
            *list = to_add;
        }
        else /* find right place */ 
        {
            //FIXME: Subsort groups by timestamp
            GlyrMemCache * tail = head;
            while(head && head->rating < to_add->rating)
            {
                tail = head;
                head = head->next;
            }

            GlyrMemCache * iter = tail;
            int cRating = tail->rating;
            int my_ctr = 0;
            while(iter && iter->rating == cRating)
            {
                my_ctr++;
                iter = iter->prev;
            }

            if(my_ctr != 1)
            {
                printf("%d INROW\n",my_ctr);
            }


            if(head != NULL)
            {
                GlyrMemCache * prev = head->prev;
                if(prev != NULL)
                    prev->next = to_add;
                
                to_add->prev = prev;
                to_add->next = head;

                head->prev = to_add;

                if(prev == NULL)
                    *list = to_add;
            }
            else /* We're at the end */
            {
                tail->next = to_add;
                to_add->prev = tail; 
            }
        }
    }
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static int select_callback(void * result, int argc, char ** argv, char ** azColName)
{
#if DO_PROFILE
    g_timer_start(select_callback_timer);
#endif


    int rc = 0;
    select_callback_data * data = result;
    GlyrMemCache ** list = data->result;

    if(argc >= 15 && data->counter < data->query->number)
    {
        GlyrMemCache * cache = DL_init();
        if(cache != NULL)
        {
            cache->prov = g_strdup(argv[3]);
            cache->dsrc = g_strdup(argv[4]);
            cache->img_format = g_strdup(argv[5]);

            cache->duration = (argv[6] ? strtol(argv[6],NULL,10)   : 0);
            cache->type =     (argv[8] ? strtol(argv[8],NULL,10)   : 0);
            cache->size =     (argv[9] ? strtol(argv[9],NULL,10)   : 0);
            cache->is_image = (argv[10] ? strtol(argv[10],NULL,10) : 0);

            if(argv[11] != NULL)
            {
                memcpy(cache->md5sum,argv[11],16);
            }

            if(argv[12] != NULL && cache->size > 0)
            {
                cache->data = g_malloc0(cache->size + 1);
                memcpy(cache->data,argv[12],cache->size);
                cache->data[cache->size] = 0;
            }

            cache->rating = (argv[13] ? strtol(argv[13],NULL,10) : 0);
            cache->timestamp = (argv[14] ? strtod(argv[14],NULL) : 0);
            cache->cached = TRUE;

            if(list != NULL)
            {
                add_to_cache_list(list,cache);
            }
            else if(data->cb != NULL && cache)
            {
                GlyrQuery q;
                glyr_query_init(&q);

                if(argv[7] != NULL)
                {
                    GLYR_GET_TYPE type = strtol(argv[7],NULL,10);
                    glyr_opt_type(&q,type);
                }

                glyr_opt_artist(&q,argv[0]);
                glyr_opt_album(&q, argv[1]);
                glyr_opt_title(&q, argv[2]);

                rc = data->cb(&q,cache,data->userptr);

                glyr_query_destroy(&q);
                DL_free(cache);
            }
        }
    }

    data->counter++;
#if DO_PROFILE
    g_timer_stop(select_callback_timer);
    select_callback_spent += g_timer_elapsed(select_callback_timer,NULL);
#endif
    return rc;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static gchar * convert_from_option_to_sql(GlyrQuery * q)
{
    gchar * result = g_strdup("'none'");

    for(GList * elem = r_getSList(); elem; elem = elem->next)
    {
        MetaDataSource * item = elem->data;
        if(item && (q->type == item->type || item->type == GLYR_GET_ANY))
        {
            if(provider_is_enabled(q,item) == TRUE)
            {
                gchar * old_mem = result;
                result = g_strdup_printf("%s%s'%s'",result,(*result) ? "," : "",item->name);
                g_free(old_mem);
            }
        }
    }
    return result;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static GLYR_FIELD_REQUIREMENT get_req(GlyrQuery * q)
{
    return (q) ? glyr_get_requirements(q->type) : 0;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static int delete_callback(void * result, int argc, char ** argv, char ** azColName)
{
    delete_callback_data * data = result;
    if(argc >= 4 && result && data->max_delete > data->deleted)
    {
        /* God, this is so silly.. SQL, why you don't like " = null"
         * I can't think of any easier way to do this, tell me if you found one
         */
        gchar * sql_delete = sqlite3_mprintf(
                "DELETE FROM metadata WHERE \n"
                "get_type    %s %s AND \n"
                "artist_id   %s %s AND \n"
                "album_id    %s %s AND \n"
                "title_id    %s %s AND \n"
                "provider_id %s %s;\n",
                argv[0] ? "=" : " IS ", argv[0] ? argv[0] : "NULL",
                argv[1] ? "=" : " IS ", argv[1] ? argv[1] : "NULL",
                argv[2] ? "=" : " IS ", argv[2] ? argv[2] : "NULL",
                argv[3] ? "=" : " IS ", argv[3] ? argv[3] : "NULL",
                argv[4] ? "=" : " IS ", argv[4] ? argv[4] : "NULL");

        if(sql_delete != NULL)
        {
            execute(data->con,sql_delete);
            sqlite3_free(sql_delete);
            data->deleted++;
        }
    }
    return 0;
}
