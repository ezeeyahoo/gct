/******************************************************************************
gass_cache.c 

Description:

CVS Information:

    $Source$
    $Date$
    $Revision$
    $Author$
******************************************************************************/

/******************************************************************************
                             Include header files
******************************************************************************/
#include "globus_common.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "lber.h"
#include "ldap.h"
#include "globus_gram_client.h"
#include "globus_gass_server_ez.h"
#include "globus_gass_client.h"
#include "globus_gass_cache.h"
#include "globus_nexus.h"

/******************************************************************************
                             Type definitions
******************************************************************************/
typedef enum {
    GASSL_ADD,
    GASSL_DELETE,
    GASSL_CLEANUP_TAG,
    GASSL_CLEANUP_FILE,
    GASSL_LIST,
    GASSL_QUERY_URL
} globus_l_cache_op_t;

/******************************************************************************
                             Module specific prototypes
******************************************************************************/
static char *globus_l_cache_get_contact_string(LDAP *ldap_server,
						    LDAPMessage* entry);

static int globus_l_cache_usage(void);
static char *globus_l_cache_get_rm_contact(char *resource);
static void globus_l_cache_remote_op(globus_l_cache_op_t op,
			             char *tag,
			             char *url,
			             char *rm_contact);
static void globus_l_cache_local_op(globus_l_cache_op_t op,
			            char *tag,
			            char *url);
static void globus_l_cache_print_url(globus_gass_cache_entry_t *entry,
		                     char *tag);
static char *globus_l_cache_tag_arg(char *tag);
static char *globus_l_cache_url_arg(char *url);
/******************************************************************************
			     Module specific variables
******************************************************************************/
static globus_mutex_t globus_l_cache_monitor_mutex;
static globus_cond_t  globus_l_cache_monitor_cond;
static globus_bool_t  globus_l_cache_monitor_done = GLOBUS_FALSE;

#define GLOBUS_CACHE_USAGE \
"usage: globus-gass-cache operation [-r resource] [-t tag] [URL]\n"\
"       globus-gass-cache -v\n"\
"valid values for \"operation\" are:\n"\
"    add            - add an URL to the cache\n"\
"                     This operation requires that an URL be specified on\n"\
"                     the command line. If the [-t tag] option is specified,\n"\
"                     then the URL will be added with that tag; Otherwise \n"\
"                     the default \"null\" tag will be used.\n"\
"    delete         - decrement the reference count for an URL\n"\
"                     This operation requires that an URL be specified on\n"\
"                     the command line. If the [-t tag] option is specified,\n"\
"                     then a single instance of that tag will be removed\n"\
"                     from that URLs cache entry; Otherwise, a single\n"\
"                     reference to the default \"null\" tag will be removed\n"\
"    cleanup_tag    - remove all references to a tag from the cache.\n"\
"                     If an URL is specified, then all references to the\n"\
"                     tag will be removed from that URL. If no tag is\n"\
"                     specified by the [-t tag] option, then the \"null\"\n"\
"                     tag will be used.\n"\
"    cleanup_url    - remove all tags for an URL in the cache\n"\
"                     This operation requires that the URL be specified on\n"\
"                     the command line.\n"\
"    list           - list the contents of the cache.\n"\
"                     If either the [-t tag] or the URL are specified on the\n"\
"                     command line, then only cache entries which match\n"\
"                     those will be listed\n"\
"\n"\
"optional arguments:\n"\
"    -v             - Display the program version and exit\n"\
"    -t tag         - The string passed as the tag argument will be used\n"\
"                     as described above\n"\
"    -r resource    - The resource argument specifies that the cache\n"\
"                     operation will be performed on a remote cache. The\n"\
"                     resource string can be either a resource manager name\n"\
"                     located in the MDS, or a resource manager contact\n"\
"                     string.\n"

/******************************************************************************
Function: main()

Description:

Parameters: 

Returns: 
******************************************************************************/
int
main(int argc, char **argv)
{
    int i;
    globus_l_cache_op_t op;
    int arg = 1;
    char *resource    = GLOBUS_NULL,
	 *url         = GLOBUS_NULL,
	 *rm_contact  = GLOBUS_NULL,
	 *tag         = GLOBUS_NULL;

    if(argc < 2)
    {
	return globus_l_cache_usage();
    }
    if(argc == 2 &&
       strcmp(argv[1], "-v") == 0)
    {
	if(GLOBUS_RELEASE_NOT_BETA)
	{
	    globus_libc_printf("Version %d.%d.%d,\n"
			       "GASS Protocol Version %d\n"
			       "GASS Cache File Version %d\n",
			       globus_release_major(),
			       globus_release_minor(),
			       globus_release_patch(),
			       GLOBUS_GASS_PROTO_VERSION,
			       GLOBUS_GASS_CACHE_STATE_FILE_FORMAT_VERSION);
	}
	else
	{
	   globus_libc_printf("Version %d.%d.%db%d\n"
			      "GASS Protocol Version %d\n"
			      "GASS Cache File Version %d\n",
			      globus_release_major(),
			      globus_release_minor(),
			      globus_release_patch(),
			      globus_release_beta(),
			      GLOBUS_GASS_PROTO_VERSION,
			      GLOBUS_GASS_CACHE_STATE_FILE_FORMAT_VERSION);

	}
	exit(0);
    }

    /* parse op */
    if(strncmp("add", argv[arg], strlen(argv[arg])) == 0)
    {
	op = GASSL_ADD;
    }
    else if(strncmp("delete", argv[arg], strlen(argv[arg])) == 0)
    {
	op = GASSL_DELETE;
    }
    else if(
	strncmp("cleanup_tag", argv[arg], strlen(argv[arg])) == 0 &&
	strncmp("cleanup_url", argv[arg], strlen(argv[arg])) != 0)
    {
	op = GASSL_CLEANUP_TAG;
    }
    else if(
	strncmp("cleanup_url", argv[arg], strlen(argv[arg])) == 0 &&
	strncmp("cleanup_tag", argv[arg], strlen(argv[arg])) != 0)
    {
	op = GASSL_CLEANUP_FILE;
    }
    else if(strncmp("list", argv[arg], strlen(argv[arg])) == 0)
    {
	op = GASSL_LIST;
    }
    else if(strncmp("query", argv[arg], strlen(argv[arg])) == 0)
    {
	op = GASSL_QUERY_URL;
    }
    else
    {
	return globus_l_cache_usage();
    }

    arg++;

    /* parse options [-t tag] [-r resource] */
    while(arg < argc)
    {
	if(strcmp("-t", argv[arg]) == 0)
	{
	    arg++;
	    if(arg == argc)
	    {
		return globus_l_cache_usage();
	    }
	    tag = argv[arg++];
	}
	else if(strcmp("-r", argv[arg]) == 0)
	{
	    arg++;
	    if(arg == argc)
	    {
		return globus_l_cache_usage();
	    }
	    resource = argv[arg++];
	}
	else
	{
	    if(url != GLOBUS_NULL)
	    {
		return globus_l_cache_usage();
	    }
	    url = argv[arg++];
	}
    }

    /* verify globus_l_cache_usage */
    if(op == GASSL_ADD ||
       op == GASSL_DELETE ||
       op == GASSL_CLEANUP_FILE ||
       op == GASSL_QUERY_URL)
    {
	if(url == GLOBUS_NULL)
	{
	    return globus_l_cache_usage();
	}
    }
    
    if(resource != GLOBUS_NULL)
    {
	rm_contact = globus_l_cache_get_rm_contact(resource);
	if(rm_contact == GLOBUS_NULL)
	{
	    globus_libc_printf("Couldn't find resource %s\n", resource);
	    exit(1);
	}
    }

    if(rm_contact != GLOBUS_NULL)
    {
	globus_l_cache_remote_op(op, tag, url, rm_contact);
    }
    else
    {
	globus_l_cache_local_op(op, tag, url);
    }
    return 0;
} /* main() */

/******************************************************************************
Function: globus_l_cache_get_rm_contact()

Description:

Parameters: 

Returns: 
******************************************************************************/
static char *
globus_l_cache_get_rm_contact(char *resource)
{
    LDAP *ldap_server;
    int default_port          = GLOBUS_MDS_PORT;
    int port		      = 0;
    char *default_base_dn     = GLOBUS_MDS_ROOT_DN;
    char *base_dn	      = GLOBUS_NULL;
    char *default_server      = GLOBUS_MDS_HOST;
    char *server	      = GLOBUS_NULL;
    char *search_string;
    LDAPMessage *reply;
    LDAPMessage *entry;
    char *attrs[3];
    FILE *mds_conf;
    char buf[512];
    
    if(strchr(resource, (int) ':') != GLOBUS_NULL)
    {
	return strdup(resource);
    }

    
    mds_conf = fopen(GLOBUS_SYSCONFDIR "/globus-mds.conf", "r");
    if(mds_conf != GLOBUS_NULL)
    {
	while(fgets(buf, 512, mds_conf) != GLOBUS_NULL)
	{
	    /* break off comments */
	    strtok(buf, "#");
	    if(strlen(buf) > 0)
	    {
		if(strncmp(buf,
			   "GLOBUS_MDS_HOST=",
			   strlen("GLOBUS_MDS_HOST=")) == 0)
		{
		    globus_libc_lock();
		    server=strdup(buf+strlen("GLOBUS_MDS_HOST=\""));
		    server[strlen(server)-2] = '\0';
		    globus_libc_unlock();
		}
		else if(strncmp(buf,
				"GLOBUS_MDS_PORT=",
				strlen("GLOBUS_MDS_PORT=")) == 0)
		{
		    port = atoi(buf + strlen("GLOBUS_MDS_PORT=\""));
		}
		else if(strncmp(buf,
				"GLOBUS_MDS_DN=",
				strlen("GLOBUS_MDS_DN=")) == 0)
		{
		    globus_libc_lock();
		    base_dn = strdup(buf + strlen("GLOBUS_MDS_DN=\""));
		    base_dn[strlen(base_dn)-2] = '\0';
		    globus_libc_unlock();
		}
	    }
	}
	fclose(mds_conf);
    }
    /* fall back to defaults if we can't parse the file, or the file
       can't be opened */
    if(server == GLOBUS_NULL)
    {
	server = default_server;
    }
    if(port == 0)
    {
	port = default_port;
    }
    if(base_dn == GLOBUS_NULL)
    {
	base_dn = default_base_dn;
    }
    
    attrs[0] = "contact";
    attrs[1] = GLOBUS_NULL;
    
    if((ldap_server = ldap_open(server, port)) == GLOBUS_NULL)
    {
	ldap_perror(ldap_server, "ldap_open");
	exit(1);
    }

    if(ldap_simple_bind_s(ldap_server, "", "") != LDAP_SUCCESS)
    {
	ldap_perror(ldap_server, "ldap_simple_bind_s");
	ldap_unbind(ldap_server);
	exit(1);
    }

    search_string=malloc(strlen(resource)+5);

    globus_libc_sprintf(search_string, "cn=%s", resource);
    
    if(ldap_search_s(ldap_server,
		     base_dn,
		     LDAP_SCOPE_SUBTREE,
		     search_string,
		     attrs,
		     0,
		     &reply) != LDAP_SUCCESS)
    {
	ldap_perror(ldap_server, "ldap_search");
	ldap_unbind(ldap_server);
	exit(1);
    }

    for(entry = ldap_first_entry(ldap_server, reply);
	entry != GLOBUS_NULL;
	entry = ldap_next_entry(ldap_server, entry))
    {
	char *contact;
	contact = globus_l_cache_get_contact_string(ldap_server, entry);
	if(contact != GLOBUS_NULL)
	{
	    ldap_unbind(ldap_server);

	    if(server != default_server)
	    {
		globus_free(server);
	    }
	    if(base_dn != default_base_dn)
	    {
		globus_free(base_dn);
	    }
	    globus_free(search_string);
	    return contact;
	}
    }
    ldap_msgfree(reply);
    ldap_unbind(ldap_server);

    if(server != default_server)
    {
	globus_free(server);
    }
    if(base_dn != default_base_dn)
    {
	globus_free(base_dn);
    }
    globus_free(search_string);
    return GLOBUS_NULL;
} /* globus_l_cache_get_rm_contact() */

/******************************************************************************
Function: globus_l_cache_usage()

Description:

Parameters: 

Returns: 
******************************************************************************/
static int
globus_l_cache_usage(void)
{
    globus_libc_printf(GLOBUS_CACHE_USAGE);

    return 1;
} /* globus_l_cache_usage() */

/******************************************************************************
Function: globus_l_cache_get_contact_string()

Description:

Parameters: 

Returns: 
******************************************************************************/
static char *
globus_l_cache_get_contact_string(LDAP *ldap_server, LDAPMessage* entry)
{
    char *a, *dn;
    BerElement *ber;
    char** values;
    int numValues;
    int i;
    char *contact=GLOBUS_NULL;

    for (a = ldap_first_attribute(ldap_server,entry,&ber); a != NULL;
	 a = ldap_next_attribute(ldap_server,entry,ber) )
    {
	values = ldap_get_values(ldap_server,entry,a);
	numValues = ldap_count_values(values);
	
	if(strcmp(a, "contact") == 0)
	{
	    contact = strdup(values[0]);
	    ldap_value_free(values);
	    break;
	}
    }
    return contact;
} /* globus_l_cache_get_contact_string() */

/******************************************************************************
Function: globus_l_cache_url_arg()

Description:

Parameters: 

Returns: 
******************************************************************************/
static char *
globus_l_cache_url_arg(char *url)
{
    static char arg[1024];

    /* globus_libc_lock is acquired before this is called */
    if(url != GLOBUS_NULL)
    {
	sprintf(arg,
	        "\"%s\"",
	        url);
    }
    else
    {
	arg[0]='\0';
    }

    return arg;
} /* globus_l_cache_url_arg() */

/******************************************************************************
Function: globus_l_cache_tag_arg()

Description:

Parameters: 

Returns: 
******************************************************************************/
static char *
globus_l_cache_tag_arg(char *tag)
{
    static char arg[1024];

    /* globus_libc_lock is acquired before this is called */
    if(tag != GLOBUS_NULL)
    {
	sprintf(arg,
	        "-t \"%s\"",
	        tag);
    }
    else
    {
	arg[0]='\0';
    }

    return arg;
} /* globus_l_cache_tag_arg() */

/******************************************************************************
Function: globus_l_cache_op_string()

Description:

Parameters: 

Returns: 
******************************************************************************/
static char *
globus_l_cache_op_string(globus_l_cache_op_t op)
{
    switch(op)
    {
    case GASSL_ADD:
	return "add";
    case GASSL_DELETE:
	return "delete";
    case GASSL_CLEANUP_TAG:
	return "cleanup_tag";
    case GASSL_CLEANUP_FILE:
	return "cleanup_url";
    case GASSL_LIST:
	return "list";
    default:
	return "";
    }
} /* globus_l_cache_op_string() */

/******************************************************************************
Function: globus_l_cache_callback_func()
Description:

Parameters: 

Returns: 
******************************************************************************/
static void
globus_l_cache_callback_func(void *arg,
	      char *job_contact,
	      int state,
	      int errorcode)
{
    if(state == GLOBUS_GRAM_CLIENT_JOB_STATE_FAILED ||
       state == GLOBUS_GRAM_CLIENT_JOB_STATE_DONE)
    {
	globus_mutex_lock(&globus_l_cache_monitor_mutex);
	globus_l_cache_monitor_done = GLOBUS_TRUE;
	globus_cond_signal(&globus_l_cache_monitor_cond);
	globus_mutex_unlock(&globus_l_cache_monitor_mutex);
    }
} /* globus_l_cache_callback_func() */

/******************************************************************************
Function: globus_l_cache_remote_op()

Description:

Parameters: 

Returns: 
******************************************************************************/
static void
globus_l_cache_remote_op(globus_l_cache_op_t op,
	     char *tag,
	     char *url,
	     char *rm_contact)
{
    char spec[1024];
    char *server_url;
    unsigned short port=0;
    char *callback_contact;
    char *job_contact;
    int rc;
    
    globus_module_activate(GLOBUS_GRAM_CLIENT_MODULE);
    globus_gram_client_callback_allow(globus_l_cache_callback_func,
			              GLOBUS_NULL,
			              &callback_contact);
    
    globus_gass_server_ez_init(&port,
			       &server_url,
			       GLOBUS_GASS_SERVER_EZ_STDOUT_ENABLE|
			       GLOBUS_GASS_SERVER_EZ_STDERR_ENABLE|
			       GLOBUS_GASS_SERVER_EZ_LINE_BUFFER,
			       (globus_gass_server_ez_client_shutdown_t) GLOBUS_NULL);


    globus_libc_lock();
    sprintf(spec,
	    "&(executable=$(GLOBUS_SERVICES_PREFIX)/bin/globus-gass-cache)"
	    " (stdout=%s/dev/stdout)"
	    " (stderr=%s/dev/stdout)"
	    " (stdin=/dev/null)"
	    " (arguments=%s %s %s)",
	    server_url,
	    server_url,
	    globus_l_cache_op_string(op),
	    globus_l_cache_tag_arg(tag),
	    globus_l_cache_url_arg(url));
    globus_libc_unlock();

    globus_mutex_init(&globus_l_cache_monitor_mutex, GLOBUS_NULL);
    globus_cond_init(&globus_l_cache_monitor_cond, GLOBUS_NULL);

    globus_mutex_lock(&globus_l_cache_monitor_mutex);
    
    rc = globus_gram_client_job_request(rm_contact,
			    spec,
			    31,
			    callback_contact,
			    &job_contact);
				
    if(rc != GLOBUS_SUCCESS)
    {
	globus_libc_printf("Error submitting remote cache request\n");
	return ;
    }
    while(!globus_l_cache_monitor_done)
    {
	globus_cond_wait(&globus_l_cache_monitor_cond, &globus_l_cache_monitor_mutex);
    }
    globus_mutex_unlock(&globus_l_cache_monitor_mutex);
    globus_module_deactivate(GLOBUS_GRAM_CLIENT_MODULE);
    globus_gass_server_ez_shutdown(port);
} /* globus_l_cache_remote_op() */

/******************************************************************************
Function: globus_l_cache_local_op()

Description:

Parameters: 

Returns: 
******************************************************************************/
static void
globus_l_cache_local_op(globus_l_cache_op_t op,
	    char *tag,
	    char *url)
{
    globus_gass_cache_t cache_handle;
    unsigned long timestamp;
    char *local_filename;
    int rc;
    globus_gass_cache_entry_t *entries;
    int size=0;
    int i, j;
    
    rc = globus_module_activate(GLOBUS_GASS_CLIENT_MODULE);

    if(rc != GLOBUS_SUCCESS)
    {
	globus_libc_printf("Error %d activating GASS client library\n",
			   rc);
	return;
    }
    
    rc = globus_gass_cache_open(GLOBUS_NULL, &cache_handle);
    if(rc != GLOBUS_SUCCESS)
    {
	globus_libc_printf("Error (%d: %s) opening GASS cache\n",
			   rc,
			   globus_gass_cache_error_string(rc));
	return;
    }
    
    switch(op)
    {
    case GASSL_ADD:
	rc = globus_gass_cache_add(&cache_handle,
			    url,
			    tag,
			    GLOBUS_TRUE,
			    &timestamp,
			    &local_filename);
	if(rc == GLOBUS_GASS_CACHE_ADD_EXISTS)
	{
	    globus_gass_cache_add_done(&cache_handle,
				url,
				tag,
				timestamp);
	}
	else if(rc == GLOBUS_GASS_CACHE_ADD_NEW)
	{
	    int fd = open(local_filename, O_WRONLY|O_TRUNC);

	    globus_gass_client_get_fd(url,
			       GLOBUS_NULL,
			       fd,
			       GLOBUS_GASS_LENGTH_UNKNOWN,
			       &timestamp,
			       GLOBUS_NULL,
			       GLOBUS_NULL);
	    close(fd);
	    globus_gass_cache_add_done(&cache_handle,
				url,
				tag,
				timestamp);
	}
	free(local_filename);
	break;
    case GASSL_DELETE:
	globus_gass_cache_delete_start(&cache_handle,
				url,
				tag,
				&timestamp);
	globus_gass_cache_delete(&cache_handle,
			  url,
			  tag,
			  timestamp,
			  GLOBUS_TRUE);
	break;
    case GASSL_CLEANUP_TAG:
	if(url == GLOBUS_NULL)
	{
	    globus_gass_cache_list(&cache_handle,
			    &entries,
			    &size);

            for(i = 0; i < size; i++)
	    {
		globus_gass_cache_cleanup_tag(&cache_handle,
				       entries[i].url,
				       tag);
	    }
	    globus_gass_cache_list_free(entries, size);
	}
	else
	{
	    globus_gass_cache_cleanup_tag(&cache_handle,
			           url,
			           tag);
	}
	break;
	
    case GASSL_CLEANUP_FILE:
	globus_gass_cache_cleanup_file(&cache_handle,
				url);
	break;
    case GASSL_LIST:
	globus_gass_cache_list(&cache_handle,
			&entries,
			&size);

	for(i = 0; i < size; i++)
	{
	    if(url != GLOBUS_NULL)
	    {
		if(strcmp(url, entries[i].url) == 0)
		{
		    globus_l_cache_print_url(&entries[i], tag);
		}
	    }
	    else
	    {
		globus_l_cache_print_url(&entries[i], tag);
	    }
	}
	globus_gass_cache_list_free(entries, size);
	break;
    case GASSL_QUERY_URL:
	rc = globus_gass_cache_add(&cache_handle,
				   url,
				   tag,
				   GLOBUS_FALSE, /* DO NOT CREATE */
				   &timestamp,
				   &local_filename);
	
	if(rc == GLOBUS_GASS_CACHE_ADD_EXISTS)
	{
	    globus_gass_cache_add_done(&cache_handle,
				url,
				tag,
				timestamp);
	    globus_libc_printf("%s\n",local_filename);
	}
	else if(rc == GLOBUS_GASS_CACHE_URL_NOT_FOUND)
	{
	    globus_libc_printf("\n");
	}
	else
	{
	    globus_libc_printf("Error (%d: %s) querying cache\n",
			   rc,
			   globus_gass_cache_error_string(rc));
	    return;
	}
	free(local_filename);
	break;
    }
    globus_gass_cache_close(&cache_handle);

} /* globus_l_cache_local_op() */

/******************************************************************************
Function: globus_l_cache_print_url()

Description:

Parameters: 

Returns: 
******************************************************************************/
static void
globus_l_cache_print_url(globus_gass_cache_entry_t *entry,
	  char *tag)
{
    int j;
    globus_bool_t print_all_tags=GLOBUS_FALSE;

    if(tag == GLOBUS_NULL)
    {
	print_all_tags = GLOBUS_TRUE;
    }

    printf("%s\n", entry->url);
    for(j = 0; j < entry->num_tags; j++)
    {
	if(print_all_tags || strcmp(tag, entry->tags[j].tag) == 0)
	{
	    printf("\ttag '%s' (%i refs)\n",
		   entry->tags[j].tag,
		   entry->tags[j].count);
	}
    }
} /* globus_l_cache_print_url() */
