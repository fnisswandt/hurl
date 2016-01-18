//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    main.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/hlx.h"
#include "support/tls_util.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

#include <list>
#include <set>
#include <algorithm>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "../../include/hlx/stat.h"
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Profiler
#ifdef ENABLE_PROFILER
#include <google/profiler.h>
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0

#define MAX_READLINE_SIZE 1024

// Version
#define PHURL_VERSION_MAJOR 0
#define PHURL_VERSION_MINOR 0
#define PHURL_VERSION_MACRO 1
#define PHURL_VERSION_PATCH "alpha"

//: ----------------------------------------------------------------------------
//: Status
//: ----------------------------------------------------------------------------
#ifndef STATUS_ERROR
#define STATUS_ERROR -1
#endif

#ifndef STATUS_OK
#define STATUS_OK 0
#endif

//: ----------------------------------------------------------------------------
//: ANSI Color Code Strings
//:
//: Taken from:
//: http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html
//: ----------------------------------------------------------------------------
#define ANSI_COLOR_OFF          "\033[0m"
#define ANSI_COLOR_FG_BLACK     "\033[01;30m"
#define ANSI_COLOR_FG_RED       "\033[01;31m"
#define ANSI_COLOR_FG_GREEN     "\033[01;32m"
#define ANSI_COLOR_FG_YELLOW    "\033[01;33m"
#define ANSI_COLOR_FG_BLUE      "\033[01;34m"
#define ANSI_COLOR_FG_MAGENTA   "\033[01;35m"
#define ANSI_COLOR_FG_CYAN      "\033[01;36m"
#define ANSI_COLOR_FG_WHITE     "\033[01;37m"
#define ANSI_COLOR_FG_DEFAULT   "\033[01;39m"
#define ANSI_COLOR_BG_BLACK     "\033[01;40m"
#define ANSI_COLOR_BG_RED       "\033[01;41m"
#define ANSI_COLOR_BG_GREEN     "\033[01;42m"
#define ANSI_COLOR_BG_YELLOW    "\033[01;43m"
#define ANSI_COLOR_BG_BLUE      "\033[01;44m"
#define ANSI_COLOR_BG_MAGENTA   "\033[01;45m"
#define ANSI_COLOR_BG_CYAN      "\033[01;46m"
#define ANSI_COLOR_BG_WHITE     "\033[01;47m"
#define ANSI_COLOR_BG_DEFAULT   "\033[01;49m"

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// ---------------------------------------
// Output types
// ---------------------------------------
typedef enum {
        OUTPUT_LINE_DELIMITED,
        OUTPUT_JSON
} output_type_t;

// ---------------------------------------
// Output types
// ---------------------------------------
typedef enum {
        PART_HOST = 1,
        PART_SERVER = 1 << 1,
        PART_STATUS_CODE = 1 << 2,
        PART_HEADERS = 1 << 3,
        PART_BODY = 1 << 4
} output_part_t;

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )
#define ARESP(_str) l_responses_str += _str

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::map <std::string, uint32_t> summary_map_t;
// Summary info
typedef struct summary_info_struct {
        uint32_t m_success;
        uint32_t m_error_addr;
        uint32_t m_error_addr_timeout;
        uint32_t m_error_conn;
        uint32_t m_error_unknown;
        uint32_t m_tls_error_hostname;
        uint32_t m_tls_error_self_signed;
        uint32_t m_tls_error_expired;
        uint32_t m_tls_error_issuer;
        uint32_t m_tls_error_other;
        summary_map_t m_tls_protocols;
        summary_map_t m_tls_ciphers;
        summary_info_struct():
                m_success(),
                m_error_addr(),
                m_error_addr_timeout(),
                m_error_conn(),
                m_error_unknown(),
                m_tls_error_hostname(),
                m_tls_error_self_signed(),
                m_tls_error_expired(),
                m_tls_error_issuer(),
                m_tls_error_other(),
                m_tls_protocols(),
                m_tls_ciphers()
        {};
} summary_info_t;

typedef struct phurl_resp_struct {
        ns_hlx::subr *m_subr;
        ns_hlx::resp *m_resp;
        const char *m_tls_info_cipher_str;
        const char *m_tls_info_protocol_str;
        std::string m_error_str;
        phurl_resp_struct():
                m_subr(NULL),
                m_resp(NULL),
                m_tls_info_cipher_str(NULL),
                m_tls_info_protocol_str(NULL),
                m_error_str()
        {}
private:
        // Disallow copy/assign
        phurl_resp_struct& operator=(const phurl_resp_struct &);
        phurl_resp_struct(const phurl_resp_struct &);
} phurl_resp_t;

// Host
typedef struct host_struct {
        std::string m_host;
        std::string m_hostname;
        std::string m_id;
        std::string m_where;
        std::string m_url;
        uint16_t m_port;
        host_struct():
                m_host(),
                m_hostname(),
                m_id(),
                m_where(),
                m_url(),
                m_port(0)
        {};
} host_t;

typedef std::list <phurl_resp_t *> phurl_resp_list_t;
typedef std::set <uint64_t> phurl_uid_set_t;
typedef std::list <host_t> host_list_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_stats;
        bool m_show_summary;
        ns_hlx::hlx *m_hlx;
        ns_hlx::subr *m_subr;
        host_list_t *m_host_list;
        uint32_t m_total_reqs;

        // Results
        pthread_mutex_t m_mutex;
        phurl_uid_set_t m_pending_uid_set;
        phurl_resp_list_t m_resp_list;
        summary_info_t m_summary_info;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct(void) :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_stats(false),
                m_show_summary(false),
                m_hlx(NULL),
                m_subr(NULL),
                m_host_list(NULL),
                m_total_reqs(0),
                m_mutex(),
                m_pending_uid_set(),
                m_resp_list(),
                m_summary_info()

        {
                pthread_mutex_init(&m_mutex, NULL);
        }

private:
        // Disallow copy/assign
        settings_struct& operator=(const settings_struct &);
        settings_struct(const settings_struct &);

} settings_struct_t;

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
bool g_test_finished = false;
bool g_cancelled = false;
settings_struct_t *g_settings = NULL;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void display_status_line(settings_struct_t &a_settings);
void display_summary(settings_struct_t &a_settings, uint32_t a_num_hosts);
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len);
std::string dump_all_responses(phurl_resp_list_t &a_resp_list, bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map);

//: ----------------------------------------------------------------------------
//: \details: Completion callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_completion_cb(ns_hlx::subr &a_subr,
                               ns_hlx::nconn &a_nconn,
                               ns_hlx::resp &a_resp)
{
        settings_struct_t *l_settings = static_cast<settings_struct_t *>(a_subr.get_data());
        if(!l_settings)
        {
                return -1;
        }

        phurl_resp_t *l_resp = new phurl_resp_t();
        l_resp->m_resp = &a_resp;
        l_resp->m_subr = &a_subr;

        SSL *l_SSL = ns_hlx::nconn_get_SSL(a_nconn);
        if(l_SSL)
        {
                int32_t l_protocol_num = ns_hlx::get_tls_info_protocol_num(l_SSL);
                l_resp->m_tls_info_cipher_str = ns_hlx::get_tls_info_cipher_str(l_SSL);
                l_resp->m_tls_info_protocol_str = ns_hlx::get_tls_info_protocol_str(l_protocol_num);
#if 0
                SSL_SESSION *m_tls_session = SSL_get_session(m_tls);
                SSL_SESSION_print_fp(stdout, m_tls_session);
                X509* l_cert = NULL;
                l_cert = SSL_get_peer_certificate(m_tls);
                if(NULL == l_cert)
                {
                        NDBG_PRINT("SSL_get_peer_certificate error.  tls: %p\n", m_tls);
                        return NC_STATUS_ERROR;
                }
                X509_print_fp(stdout, l_cert);
#endif
        }

        pthread_mutex_lock(&(l_settings->m_mutex));
        l_settings->m_pending_uid_set.erase(a_subr.get_uid());
        l_settings->m_resp_list.push_back(l_resp);
        ++(l_settings->m_summary_info.m_success);
        if(l_resp->m_tls_info_cipher_str) ++(l_settings->m_summary_info.m_tls_ciphers[l_resp->m_tls_info_cipher_str]);
        if(l_resp->m_tls_info_protocol_str) ++(l_settings->m_summary_info.m_tls_protocols[l_resp->m_tls_info_protocol_str]);
        pthread_mutex_unlock(&(l_settings->m_mutex));

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: Completion callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_error_cb(ns_hlx::subr &a_subr,
                          ns_hlx::nconn &a_nconn)
{
        settings_struct_t *l_settings = static_cast<settings_struct_t *>(a_subr.get_data());
        if(!l_settings)
        {
                return -1;
        }
        phurl_resp_t *l_resp = new phurl_resp_t();
        l_resp->m_resp = NULL;
        l_resp->m_subr = &a_subr;
        l_resp->m_error_str = ns_hlx::nconn_get_last_error_str(a_nconn);
        ns_hlx::conn_status_t l_conn_status = ns_hlx::nconn_get_status(a_nconn);
        pthread_mutex_lock(&(l_settings->m_mutex));
        ++(l_settings->m_summary_info.m_error_conn);
        switch(l_conn_status)
        {
        case ns_hlx::CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE:
        {
                ++(l_settings->m_summary_info.m_error_addr);
                break;
        }
        case ns_hlx::CONN_STATUS_ERROR_ADDR_LOOKUP_TIMEOUT:
        {
                ++(l_settings->m_summary_info.m_error_addr_timeout);
                break;
        }
        case ns_hlx::CONN_STATUS_ERROR_TLS:
        {
                // Get last error
                SSL *l_tls = ns_hlx::nconn_get_SSL(a_nconn);
                long l_tls_vr = SSL_get_verify_result(l_tls);
                if ((l_tls_vr == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
                    (l_tls_vr == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
                {
                        ++(l_settings->m_summary_info.m_tls_error_self_signed);
                }
                else if(l_tls_vr == X509_V_ERR_CERT_HAS_EXPIRED)
                {
                        ++(l_settings->m_summary_info.m_tls_error_expired);
                }
                else if((l_tls_vr == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) ||
                        (l_tls_vr == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY))
                {
                        ++(l_settings->m_summary_info.m_tls_error_issuer);
                }
                else
                {
                        //long l_err = ns_hlx::nconn_get_last_SSL_err(a_nconn);
                        //printf("ERRORXXXX: %ld\n", l_err);
                        //printf("ERRORXXXX: %s\n", ERR_error_string(l_err,NULL));
                        //printf("ERRORXXXX: %s\n", l_resp->m_error_str.c_str());
                        ++(l_settings->m_summary_info.m_tls_error_other);
                }
                break;

        }
        case ns_hlx::CONN_STATUS_ERROR_TLS_HOST:
        {
                ++(l_settings->m_summary_info.m_tls_error_hostname);
                break;
        }
        default:
        {
                ++(l_settings->m_summary_info.m_error_unknown);
                break;
        }
        }

        l_settings->m_pending_uid_set.erase(a_subr.get_uid());
        l_settings->m_resp_list.push_back(l_resp);
        pthread_mutex_unlock(&(l_settings->m_mutex));
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: Signal handler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                g_test_finished = true;
                g_cancelled = true;
                g_settings->m_hlx->stop();
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int kbhit()
{
        struct timeval l_tv;
        fd_set l_fds;
        l_tv.tv_sec = 0;
        l_tv.tv_usec = 0;
        FD_ZERO(&l_fds);
        FD_SET(STDIN_FILENO, &l_fds);
        //STDIN_FILENO is 0
        select(STDIN_FILENO + 1, &l_fds, NULL, NULL, &l_tv);
        return FD_ISSET(STDIN_FILENO, &l_fds);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nonblock(int state)
{
        struct termios ttystate;
        //get the terminal state
        tcgetattr(STDIN_FILENO, &ttystate);
        if (state == NB_ENABLE)
        {
                //turn off canonical mode
                ttystate.c_lflag &= ~ICANON;
                //minimum of number input read.
                ttystate.c_cc[VMIN] = 1;
        } else if (state == NB_DISABLE)
        {
                //turn on canonical mode
                ttystate.c_lflag |= ICANON;
        }
        //set the terminal attributes.
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int command_exec(settings_struct_t &a_settings, bool a_send_stop)
{
        int i = 0;
        char l_cmd = ' ';
        //bool l_sent_stop = false;
        //bool l_first_time = true;
        nonblock(NB_ENABLE);

        //printf("Adding subr\n\n");

        g_test_finished = false;
        // ---------------------------------------
        //   Loop forever until user quits
        // ---------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {
                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {
                        // -------------------------------------------
                        // Display
                        // -only works when not reading from stdin
                        // -------------------------------------------
                        case 'd':
                        {
                                a_settings.m_hlx->display_stats();
                                break;
                        }
                        // -------------------------------------------
                        // Quit
                        // -only works when not reading from stdin
                        // -------------------------------------------
                        case 'q':
                        {
                                g_test_finished = true;
                                a_settings.m_hlx->stop();
                                //l_sent_stop = true;
                                break;
                        }
                        // -------------------------------------------
                        // Default
                        // -------------------------------------------
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(500000);

                if(a_settings.m_show_stats)
                {
                        display_status_line(a_settings);
                }

                pthread_mutex_lock(&(a_settings.m_mutex));
                if(a_settings.m_pending_uid_set.empty())
                {
                        g_test_finished = true;
                }
                pthread_mutex_unlock(&(a_settings.m_mutex));
                //if (!a_settings.m_hlx->is_running())
                //{
                //        //printf("IS NOT RUNNING.\n");
                //        g_test_finished = true;
                //}
        }

        // Send stop -if unsent
        //if(!l_sent_stop && a_send_stop)
        //{
        //        a_settings.m_hlx->stop();
        //        l_sent_stop = true;
        //}
        // wait for completion...
        //a_settings.m_hlx->wait_till_stopped();
        // One more status for the lovers
        if(a_settings.m_show_stats)
        {
                display_status_line(a_settings);
        }
        nonblock(NB_DISABLE);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_line(FILE *a_file_ptr, host_list_t &a_host_list)
{
        char l_readline[MAX_READLINE_SIZE];
        while(fgets(l_readline, sizeof(l_readline), a_file_ptr))
        {
                size_t l_readline_len = strnlen(l_readline, MAX_READLINE_SIZE);
                if(MAX_READLINE_SIZE == l_readline_len)
                {
                        // line was truncated
                        // Bail out -reject lines longer than limit
                        // -host names ought not be too long
                        printf("Error: hostnames must be shorter than %d chars\n", MAX_READLINE_SIZE);
                        return STATUS_ERROR;
                }
                // read full line
                // Nuke endline
                l_readline[l_readline_len - 1] = '\0';
                std::string l_string(l_readline);
                l_string.erase( std::remove_if( l_string.begin(), l_string.end(), ::isspace ), l_string.end() );
                host_t l_host;
                l_host.m_host = l_string;
                if(!l_string.empty())
                        a_host_list.push_back(l_host);
                //printf("READLINE: %s\n", l_readline);
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "phurl HTTP Parallel Curl.\n");
        fprintf(a_stream, "Copyright (C) 2015 Verizon Digital Media.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        PHURL_VERSION_MAJOR,
                        PHURL_VERSION_MINOR,
                        PHURL_VERSION_MACRO,
                        PHURL_VERSION_PATCH);
        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: phurl -u [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "URL Options -or without parameter\n");
        fprintf(a_stream, "  -u, --url            URL -REQUIRED (unless running cli: see --cli option).\n");
        fprintf(a_stream, "  -d, --data           HTTP body data -supports bodies up to 8k.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Hostname Input Options -also STDIN:\n");
        fprintf(a_stream, "  -f, --host_file      Host name file.\n");
        fprintf(a_stream, "  -J, --host_json      Host listing json format.\n");
        fprintf(a_stream, "  -x, --execute        Script to execute to get host names.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel.\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -R, --recv_buffer    Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer    Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay       Socket TCP no-delay.\n");
        fprintf(a_stream, "  -k, --no_cache       Don't use addr info cache.\n");
        fprintf(a_stream, "  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).\n");
        fprintf(a_stream, "  -C, --connect_only   Only connect -do not send request.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "SSL Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -O, --tls_options    SSL Options string.\n");
        fprintf(a_stream, "  -K, --tls_verify     Verify server certificate.\n");
        fprintf(a_stream, "  -N, --tls_sni        Use SSL SNI.\n");
        fprintf(a_stream, "  -B, --tls_self_ok    Allow self-signed certificates.\n");
        fprintf(a_stream, "  -M, --tls_no_host    Skip host name checking.\n");
        fprintf(a_stream, "  -F, --tls_ca_file    SSL CA File.\n");
        fprintf(a_stream, "  -L, --tls_ca_path    SSL CA Path.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --color          Color\n");
        fprintf(a_stream, "  -q, --quiet          Suppress output\n");
        fprintf(a_stream, "  -s, --show_progress  Show progress\n");
        fprintf(a_stream, "  -m, --show_summary   Show summary output\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Output Options: -defaults to line delimited\n");
        fprintf(a_stream, "  -o, --output         File to write output to. Defaults to stdout\n");
        fprintf(a_stream, "  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line\n");
        fprintf(a_stream, "  -j, --json           JSON { <HOST>: \"body\": <RESPONSE> ...\n");
        fprintf(a_stream, "  -P, --pretty         Pretty output\n");
        fprintf(a_stream, "  \n");
#ifdef ENABLE_PROFILER
        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");
#endif
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Note: If running large jobs consider enabling tcp_tw_reuse -eg:\n");
        fprintf(a_stream, "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");
        fprintf(a_stream, "\n");
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
        settings_struct_t l_settings;
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_settings.m_hlx = l_hlx;

        // For sighandler
        g_settings = &l_settings;

        l_hlx->set_collect_stats(false);
        l_hlx->set_use_ai_cache(true);

        // -------------------------------------------------
        // Subrequest settings
        // -------------------------------------------------
        ns_hlx::subr *l_subr = new ns_hlx::subr();
        l_settings.m_subr = l_subr;
        host_list_t *l_host_list = new host_list_t();
        l_settings.m_host_list = l_host_list;

        l_subr->set_save(true);
        l_subr->set_detach_resp(true);

        // Setup default headers before the user
        l_subr->set_header("User-Agent", "Verizon Digital Media Parallel Curl phurl ");
        l_subr->set_header("Accept", "*/*");
        //l_hlx->set_header("User-Agent", "ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //l_hlx->set_header("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //l_hlx->set_header("x-select-backend", "self");
        //l_hlx->set_header("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //l_hlx->set_header("Accept-Encoding", "gzip,deflate");
        //l_subr->set_header("Connection", "keep-alive");
        l_subr->set_completion_cb(s_completion_cb);
        l_subr->set_error_cb(s_error_cb);
        //l_subr->set_keepalive(true);

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'V' },
                { "url",            1, 0, 'u' },
                { "data",           1, 0, 'd' },
                { "host_file",      1, 0, 'f' },
                { "host_file_json", 1, 0, 'J' },
                { "execute",        1, 0, 'x' },
                { "parallel",       1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
                { "timeout",        1, 0, 'T' },
                { "recv_buffer",    1, 0, 'R' },
                { "send_buffer",    1, 0, 'S' },
                { "no_delay",       1, 0, 'D' },
                { "no_cache",       0, 0, 'k' },
                { "ai_cache",       1, 0, 'A' },
                { "connect_only",   0, 0, 'C' },
                { "cipher",         1, 0, 'y' },
                { "tls_options",    1, 0, 'O' },
                { "tls_verify",     0, 0, 'K' },
                { "tls_sni",        0, 0, 'N' },
                { "tls_self_ok",    0, 0, 'B' },
                { "tls_no_host",    0, 0, 'M' },
                { "tls_ca_file",    1, 0, 'F' },
                { "tls_ca_path",    1, 0, 'L' },
                { "verbose",        0, 0, 'v' },
                { "color",          0, 0, 'c' },
                { "quiet",          0, 0, 'q' },
                { "show_progress",  0, 0, 's' },
                { "show_summary",   0, 0, 'm' },
                { "output",         1, 0, 'o' },
                { "line_delimited", 0, 0, 'l' },
                { "json",           0, 0, 'j' },
                { "pretty",         0, 0, 'P' },
#ifdef ENABLE_PROFILER
                { "gprofile",       1, 0, 'G' },
#endif
                // list sentinel
                { 0, 0, 0, 0 }
        };
#ifdef ENABLE_PROFILER
        std::string l_gprof_file;
#endif
        std::string l_execute_line;
        std::string l_host_file_str;
        std::string l_host_file_json_str;
        std::string l_url;
        std::string l_ai_cache;
        std::string l_output_file = "";

        // Defaults
        output_type_t l_output_mode = OUTPUT_JSON;
        int l_output_part = PART_HOST |
                            PART_SERVER |
                            PART_STATUS_CODE |
                            PART_HEADERS |
                            PART_BODY;
        bool l_output_pretty = false;

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        bool is_opt = false;
        for(int i_arg = 1; i_arg < argc; ++i_arg) {

                if(argv[i_arg][0] == '-') {
                        // next arg is for the option
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(l_settings.m_verbose)
                        //{
                        //      printf("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }
        }

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
#ifdef ENABLE_PROFILER
        char l_short_arg_list[] = "hVvu:d:f:J:x:y:O:KNBMF:L:p:t:H:X:T:R:S:DkA:CRcqsmo:ljPG:";
#else
        char l_short_arg_list[] = "hVvu:d:f:J:x:y:O:KNBMF:L:p:t:H:X:T:R:S:DkA:CRcqsmo:ljP";
#endif
        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                {
                        l_argument = std::string(optarg);
                }
                else
                {
                        l_argument.clear();
                }
                //printf("arg[%c=%d]: %s\n", l_opt, l_option_index, l_argument.c_str());

                switch (l_opt)
                {
                // ---------------------------------------
                // Help
                // ---------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'V':
                {
                        print_version(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // URL
                // ---------------------------------------
                case 'u':
                {
                        l_url = l_argument;
                        break;
                }
                // ---------------------------------------
                // Data
                // ---------------------------------------
                case 'd':
                {
                        // TODO Size limits???
                        int32_t l_status;
                        // If a_data starts with @ assume file
                        if(l_argument[0] == '@')
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_status = read_file(l_argument.data() + 1, &(l_buf), &(l_len));
                                if(l_status != 0)
                                {
                                        printf("Error reading body data from file: %s\n", l_argument.c_str() + 1);
                                        return -1;
                                }
                                l_subr->set_body_data(l_buf, l_len);
                        }
                        else
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_len = l_argument.length() + 1;
                                l_buf = (char *)malloc(sizeof(char)*l_len);
                                l_subr->set_body_data(l_buf, l_len);
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", l_subr->get_body_len());
                        l_subr->set_header("Content-Length", l_len_str);
                        break;
                }
                // ---------------------------------------
                // Host file
                // ---------------------------------------
                case 'f':
                {
                        l_host_file_str = l_argument;
                        break;
                }
                // ---------------------------------------
                // Host file JSON
                // ---------------------------------------
                case 'J':
                {
                        l_host_file_json_str = l_argument;
                        break;
                }
                // ---------------------------------------
                // Execute line
                // ---------------------------------------
                case 'x':
                {
                        l_execute_line = l_argument;
                        break;
                }
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        l_hlx->set_tls_client_ctx_cipher_list(l_argument);
                        break;
                }
                // ---------------------------------------
                // tls options
                // ---------------------------------------
                case 'O':
                {
                        int32_t l_status;
                        l_status = l_hlx->set_tls_client_ctx_options(l_argument);
                        if(l_status != STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }

                        break;
                }
                // ---------------------------------------
                // tls verify
                // ---------------------------------------
                case 'K':
                {
                        l_subr->set_tls_verify(true);
                        break;
                }
                // ---------------------------------------
                // tls sni
                // ---------------------------------------
                case 'N':
                {
                        l_subr->set_tls_sni(true);
                        break;
                }
                // ---------------------------------------
                // tls self signed
                // ---------------------------------------
                case 'B':
                {
                        l_subr->set_tls_self_ok(true);
                        break;
                }
                // ---------------------------------------
                // tls skip host check
                // ---------------------------------------
                case 'M':
                {
                        l_subr->set_tls_no_host_check(true);
                        break;
                }
                // ---------------------------------------
                // tls ca file
                // ---------------------------------------
                case 'F':
                {
                        l_hlx->set_tls_client_ctx_ca_file(l_argument);
                        break;
                }
                // ---------------------------------------
                // tls ca path
                // ---------------------------------------
                case 'L':
                {
                        l_hlx->set_tls_client_ctx_ca_path(l_argument);
                        break;
                }
                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        int l_num_parallel = 1;
                        //printf("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_num_parallel = atoi(optarg);
                        if (l_num_parallel < 1)
                        {
                                printf("Error parallel must be at least 1\n");
                                return -1;
                        }

                        l_hlx->set_num_parallel(l_num_parallel);
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        int l_max_threads = 1;
                        //printf("arg: --threads: %s\n", l_argument.c_str());
                        l_max_threads = atoi(optarg);
                        if (l_max_threads < 0)
                        {
                                printf("Error num-threads must be 0 or greater\n");
                                return -1;
                        }
                        l_hlx->set_num_threads(l_max_threads);
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        l_status = l_subr->set_header(l_argument);
                        if(l_status != HLX_STATUS_OK)
                        {
                                printf("Error header string[%s] is malformed\n", l_argument.c_str());
                                return -1;
                        }
                        break;
                }
                // ---------------------------------------
                // Verb
                // ---------------------------------------
                case 'X':
                {
                        if(l_argument.length() > 64)
                        {
                                printf("Error verb string: %s too large try < 64 chars\n", l_argument.c_str());
                                return -1;
                        }
                        l_subr->set_verb(l_argument);
                        break;
                }
                // ---------------------------------------
                // Timeout
                // ---------------------------------------
                case 'T':
                {
                        int l_timeout_s = -1;
                        //printf("arg: --threads: %s\n", l_argument.c_str());
                        l_timeout_s = atoi(optarg);
                        if (l_timeout_s < 1)
                        {
                                printf("Error connection timeout must be > 0\n");
                                return -1;
                        }
                        l_subr->set_timeout_ms(l_timeout_s*1000);
                        break;
                }
                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        int l_sock_opt_recv_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlx->set_sock_opt_recv_buf_size(l_sock_opt_recv_buf_size);
                        break;
                }
                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlx->set_sock_opt_send_buf_size(l_sock_opt_send_buf_size);
                        break;
                }
                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_hlx->set_sock_opt_no_delay(true);
                        break;
                }
                // ---------------------------------------
                // I'm poor -I aint got no cache!
                // ---------------------------------------
                case 'k':
                {
                        l_hlx->set_use_ai_cache(false);
                        break;
                }
                // ---------------------------------------
                // Address Info cache
                // ---------------------------------------
                case 'A':
                {
                        l_hlx->set_ai_cache(l_argument);
                        break;
                }
                // ---------------------------------------
                // connect only
                // ---------------------------------------
                case 'C':
                {
                        l_subr->set_connect_only(true);
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        l_settings.m_verbose = true;
                        l_hlx->set_verbose(true);
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        l_hlx->set_color(true);
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 's':
                {
                        l_settings.m_show_stats = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 'm':
                {
                        l_settings.m_show_summary = true;
                        break;
                }
                // ---------------------------------------
                // output file
                // ---------------------------------------
                case 'o':
                {
                        l_output_file = l_argument;
                        break;
                }
                // ---------------------------------------
                // line delimited
                // ---------------------------------------
                case 'l':
                {
                        l_output_mode = OUTPUT_LINE_DELIMITED;
                        break;
                }
                // ---------------------------------------
                // json output
                // ---------------------------------------
                case 'j':
                {
                        l_output_mode = OUTPUT_JSON;
                        break;
                }
                // ---------------------------------------
                // pretty output
                // ---------------------------------------
                case 'P':
                {
                        l_output_pretty = true;
                        break;
                }
#ifdef ENABLE_PROFILER
                // ---------------------------------------
                // Google Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_argument;
                        break;
                }
#endif
                // What???
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // Huh???
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Check for required url argument
        if(l_url.empty())
        {
                fprintf(stdout, "No URL specified.\n");
                print_usage(stdout, -1);
        }
        // else set url
        if(!l_url.empty())
        {
                l_subr->init_with_url(l_url);
        }

        // -------------------------------------------------
        // Host list processing
        // -------------------------------------------------
        // Read from command
        if(!l_execute_line.empty())
        {
                FILE *fp;
                int32_t l_status = STATUS_OK;

                fp = popen(l_execute_line.c_str(), "r");
                // Error executing...
                if (fp == NULL)
                {
                }

                l_status = add_line(fp, *l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                l_status = pclose(fp);
                // Error reported by pclose()
                if (l_status == -1)
                {
                        printf("Error: performing pclose\n");
                        return STATUS_ERROR;
                }
                // Use macros described under wait() to inspect `status' in order
                // to determine success/failure of command executed by popen()
                else
                {
                }
        }
        // Read from file
        else if(!l_host_file_str.empty())
        {
                FILE * l_file;
                int32_t l_status = STATUS_OK;

                l_file = fopen(l_host_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                l_status = add_line(l_file, *l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                //printf("ADD_FILE: DONE: %s\n", a_url_file.c_str());

                l_status = fclose(l_file);
                if (0 != l_status)
                {
                        printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        else if(!l_host_file_json_str.empty())
        {
                // TODO Create a function to do all this mess
                // ---------------------------------------
                // Check is a file
                // TODO
                // ---------------------------------------
                struct stat l_stat;
                int32_t l_status = STATUS_OK;
                l_status = stat(l_host_file_json_str.c_str(), &l_stat);
                if(l_status != 0)
                {
                        printf("Error performing stat on file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        printf("Error opening file: %s.  Reason: is NOT a regular file\n", l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }

                // ---------------------------------------
                // Open file...
                // ---------------------------------------
                FILE * l_file;
                l_file = fopen(l_host_file_json_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // ---------------------------------------
                // Read in file...
                // ---------------------------------------
                int32_t l_size = l_stat.st_size;
                int32_t l_read_size;
                char *l_buf = (char *)malloc(sizeof(char)*l_size);
                l_read_size = fread(l_buf, 1, l_size, l_file);
                if(l_read_size != l_size)
                {
                        printf("Error performing fread.  Reason: %s [%d:%d]\n",
                                        strerror(errno), l_read_size, l_size);
                        return STATUS_ERROR;
                }
                std::string l_buf_str;
                l_buf_str.assign(l_buf, l_size);
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }


                // NOTE: rapidjson assert's on errors -interestingly
                rapidjson::Document l_doc;
                l_doc.Parse(l_buf_str.c_str());
                if(!l_doc.IsArray())
                {
                        printf("Error reading json from file: %s.  Reason: data is not an array\n",
                                        l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }

                // rapidjson uses SizeType instead of size_t.
                for(rapidjson::SizeType i_record = 0; i_record < l_doc.Size(); ++i_record)
                {
                        if(!l_doc[i_record].IsObject())
                        {
                                printf("Error reading json from file: %s.  Reason: array member not an object\n",
                                                l_host_file_json_str.c_str());
                                return STATUS_ERROR;
                        }
                        host_t l_host;
                        // "host" : "coolhost.com:443",
                        // "hostname" : "coolhost.com",
                        // "id" : "DE4D",
                        // "where" : "my_house"
                        if(l_doc[i_record].HasMember("host")) l_host.m_host = l_doc[i_record]["host"].GetString();
                        else l_host.m_host = "NO_HOST";
                        if(l_doc[i_record].HasMember("hostname")) l_host.m_hostname = l_doc[i_record]["hostname"].GetString();
                        else l_host.m_hostname = "NO_HOSTNAME";
                        if(l_doc[i_record].HasMember("id")) l_host.m_id = l_doc[i_record]["id"].GetString();
                        else l_host.m_id = "NO_ID";
                        if(l_doc[i_record].HasMember("where")) l_host.m_where = l_doc[i_record]["where"].GetString();
                        else l_host.m_where = "NO_WHERE";
                        if(l_doc[i_record].HasMember("port")) l_host.m_port = l_doc[i_record]["port"].GetUint();
                        else l_host.m_port = 80;
                        l_host_list->push_back(l_host);
                }

                // ---------------------------------------
                // Close file...
                // ---------------------------------------
                l_status = fclose(l_file);
                if (STATUS_OK != l_status)
                {
                        printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        // Read from stdin
        else
        {
                int32_t l_status = STATUS_OK;
                l_status = add_line(stdin, *l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }
        }

        if(l_settings.m_verbose)
        {
                printf("Showing hostname list:\n");
                for(host_list_t::iterator i_host = l_host_list->begin(); i_host != l_host_list->end(); ++i_host)
                {
                        printf("%s\n", i_host->m_host.c_str());
                }
        }

        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }

        // Initializer hlx
        int l_status = 0;

#ifdef ENABLE_PROFILER
        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }
#endif

        for(host_list_t::iterator i_host = l_settings.m_host_list->begin();
            i_host != l_settings.m_host_list->end();
            ++i_host)
        {
                ns_hlx::subr *l_subr = new ns_hlx::subr(*(l_settings.m_subr));
                l_subr->set_uid(l_hlx->get_next_subr_uuid());
                l_subr->set_host(i_host->m_host);
                if(!i_host->m_hostname.empty())
                {
                        l_subr->set_hostname(i_host->m_hostname);
                }
                if(!i_host->m_id.empty())
                {
                        l_subr->set_id(i_host->m_id);
                }
                if(!i_host->m_where.empty())
                {
                        l_subr->set_where(i_host->m_where);
                }
                if(i_host->m_port != 0)
                {
                        l_subr->set_port(i_host->m_port);
                }
                l_subr->reset_label();
                l_subr->set_data(&l_settings);
                pthread_mutex_lock(&(l_settings.m_mutex));
                l_settings.m_pending_uid_set.insert(l_subr->get_uid());
                pthread_mutex_unlock(&(l_settings.m_mutex));
                l_hlx->pre_queue_subr(*l_subr);
        }

        // Set num to request
        l_settings.m_total_reqs = (uint32_t)l_host_list->size();

        // run...
        l_status = l_hlx->run();
        if(HLX_STATUS_OK != l_status)
        {
                return -1;
        }

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        l_status = command_exec(l_settings, true);
        if(l_status != 0)
        {
                return -1;
        }

        // Stop hlx
        l_hlx->stop();

        // wait for completion...
        l_hlx->wait_till_stopped();

#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }
#endif

        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;

        // -------------------------------------------
        // Results...
        // -------------------------------------------
        if(!g_cancelled && !l_settings.m_quiet)
        {
                bool l_use_color = l_settings.m_color;
                if(!l_output_file.empty()) l_use_color = false;
                std::string l_responses_str;
                l_responses_str = dump_all_responses(l_settings.m_resp_list, l_use_color, l_output_pretty, l_output_mode, l_output_part);
                if(l_output_file.empty())
                {
                        printf("%s\n", l_responses_str.c_str());
                }
                else
                {
                        int32_t l_num_bytes_written = 0;
                        int32_t l_status = 0;
                        // Open
                        FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                        if(l_file_ptr == NULL)
                        {
                                printf("Error performing fopen. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                        // Write
                        l_num_bytes_written = fwrite(l_responses_str.c_str(), 1, l_responses_str.length(), l_file_ptr);
                        if(l_num_bytes_written != (int32_t)l_responses_str.length())
                        {
                                printf("Error performing fwrite. Reason: %s\n", strerror(errno));
                                fclose(l_file_ptr);
                                return STATUS_ERROR;
                        }

                        // Close
                        l_status = fclose(l_file_ptr);
                        if(l_status != 0)
                        {
                                printf("Error performing fclose. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                }
        }

        // -------------------------------------------
        // Summary...
        // -------------------------------------------
        if(l_settings.m_show_summary)
        {
                display_summary(l_settings, l_host_list->size());
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        if(l_hlx)
        {
                delete l_hlx;
                l_hlx = NULL;
        }

        if(l_host_list)
        {
                delete l_host_list;
                l_host_list = NULL;
        }

        for(phurl_resp_list_t::iterator i_rx = l_settings.m_resp_list.begin();
            i_rx != l_settings.m_resp_list.end();
            ++i_rx)
        {
                if(*i_rx)
                {
                        if((*i_rx)->m_subr)
                        {
                                delete (*i_rx)->m_subr;
                                (*i_rx)->m_subr = NULL;
                        }
                        if((*i_rx)->m_resp)
                        {
                                delete (*i_rx)->m_resp;
                                (*i_rx)->m_resp = NULL;
                        }
                        delete *i_rx;
                }
        }

        if(l_subr)
        {
                delete l_subr;
                l_subr = NULL;
        }

        return 0;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_summary(settings_struct_t &a_settings, uint32_t a_num_hosts)
{
        std::string l_header_str = "";
        std::string l_protocol_str = "";
        std::string l_cipher_str = "";
        std::string l_off_color = "";

        if(a_settings.m_color)
        {
                l_header_str = ANSI_COLOR_FG_CYAN;
                l_protocol_str = ANSI_COLOR_FG_GREEN;
                l_cipher_str = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        summary_info_t &l_summary_info = a_settings.m_summary_info;
        printf("****************** %sSUMMARY%s ****************** \n", l_header_str.c_str(), l_off_color.c_str());
        printf("| total hosts:                     %u\n",a_num_hosts);
        printf("| success:                         %u\n",l_summary_info.m_success);
        printf("| error address lookup:            %u\n",l_summary_info.m_error_addr);
        printf("| error address timeout:           %u\n",l_summary_info.m_error_addr_timeout);
        printf("| error connectivity:              %u\n",l_summary_info.m_error_conn);
        printf("| error unknown:                   %u\n",l_summary_info.m_error_unknown);
        printf("| tls error cert hostname          %u\n",l_summary_info.m_tls_error_hostname);
        printf("| tls error cert self-signed       %u\n",l_summary_info.m_tls_error_self_signed);
        printf("| tls error cert expired           %u\n",l_summary_info.m_tls_error_expired);
        printf("| tls error cert issuer            %u\n",l_summary_info.m_tls_error_issuer);
        printf("| tls error other                  %u\n",l_summary_info.m_tls_error_other);

        // Sort
        typedef std::map<uint32_t, std::string> _sorted_map_t;
        _sorted_map_t l_sorted_map;
        printf("+--------------- %sSSL PROTOCOLS%s -------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = l_summary_info.m_tls_protocols.begin(); i_s != l_summary_info.m_tls_protocols.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        printf("| %-32s %u\n", i_s->second.c_str(), i_s->first);
        printf("+--------------- %sSSL CIPHERS%s ---------------- \n", l_cipher_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = l_summary_info.m_tls_ciphers.begin(); i_s != l_summary_info.m_tls_ciphers.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        printf("| %-32s %u\n", i_s->second.c_str(), i_s->first);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_status_line(settings_struct_t &a_settings)
{
        // -------------------------------------------------
        // Get results from clients
        // -------------------------------------------------

        // Get stats
        ns_hlx::t_stat_t l_total;
        a_settings.m_hlx->get_stats(l_total);
        uint32_t l_num_done = l_total.m_total_reqs;
        uint32_t l_num_resolve_active = l_total.m_num_ups_resolve_active;
        uint32_t l_num_resolve_req = l_total.m_num_ups_resolve_req;
        uint32_t l_num_resolved = l_total.m_num_ups_resolved;
        uint32_t l_num_get = l_total.m_num_ups_conn_started;
        uint32_t l_num_rx = a_settings.m_total_reqs;
        uint32_t l_num_error = l_total.m_num_errors;
        if(a_settings.m_color)
        {
                printf("Done/Req'd/Resolved/Total/Error %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, l_num_done, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, l_num_get, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, l_num_resolved, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, l_num_resolve_active, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, l_num_resolve_req, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_BLUE, l_num_rx, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, l_num_error, ANSI_COLOR_OFF);
        }
        else
        {
                printf("Done/Req'd/Resolved/Total/Error %8u / %8u / %8u / %8u / %8u / %8u / %8u\n",
                                l_num_done, l_num_get, l_num_resolved, l_num_resolve_active, l_num_resolve_req, l_num_rx, l_num_error);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len)
{
        // Check is a file
        struct stat l_stat;
        int32_t l_status = STATUS_OK;
        l_status = stat(a_file, &l_stat);
        if(l_status != 0)
        {
                printf("Error performing stat on file: %s.  Reason: %s\n", a_file, strerror(errno));
                return -1;
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                printf("Error opening file: %s.  Reason: is NOT a regular file\n", a_file);
                return -1;
        }

        // Open file...
        FILE * l_file;
        l_file = fopen(a_file,"r");
        if (NULL == l_file)
        {
                printf("Error opening file: %s.  Reason: %s\n", a_file, strerror(errno));
                return -1;
        }

        // Read in file...
        int32_t l_size = l_stat.st_size;
        *a_buf = (char *)malloc(sizeof(char)*l_size);
        *a_len = l_size;
        int32_t l_read_size;
        l_read_size = fread(*a_buf, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                printf("Error performing fread.  Reason: %s [%d:%d]\n",
                                strerror(errno), l_read_size, l_size);
                return -1;
        }

        // Close file...
        l_status = fclose(l_file);
        if (STATUS_OK != l_status)
        {
                printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                return -1;
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string dump_all_responses_line_dl(phurl_resp_list_t &a_resp_list,
                                       bool a_color,
                                       bool a_pretty,
                                       int a_part_map)
{

        std::string l_responses_str = "";
        std::string l_host_color = "";
        std::string l_server_color = "";
        std::string l_id_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        char l_buf[1024*1024];
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_server_color = ANSI_COLOR_FG_RED;
                l_id_color = ANSI_COLOR_FG_CYAN;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        int l_cur_reqlet = 0;
        for(phurl_resp_list_t::const_iterator i_rx = a_resp_list.begin();
            i_rx != a_resp_list.end();
            ++i_rx, ++l_cur_reqlet)
        {

                ns_hlx::subr *l_subr = (*i_rx)->m_subr;
                ns_hlx::resp *l_resp = (*i_rx)->m_resp;
                if(!l_subr)
                {
                        continue;
                }

                bool l_fbf = false;
                // Host
                if(a_part_map & PART_HOST)
                {
                        sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                        l_host_color.c_str(), l_off_color.c_str(),
                                        l_subr->get_host().c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Server
                if(a_part_map & PART_SERVER)
                {

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                        l_server_color.c_str(), l_server_color.c_str(),
                                        l_subr->get_host().c_str(),
                                        l_subr->get_port()
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(!l_subr->get_id().empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                l_subr->get_id().c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        if(!l_subr->get_where().empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                l_subr->get_where().c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }


                        l_fbf = true;
                }

                if(!l_resp)
                {
                        ARESP("\n");
                        continue;
                }
                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        const char *l_status_val_color = "";
                        if(a_color)
                        {
                                if(l_resp->get_status() == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                else l_status_val_color = ANSI_COLOR_FG_RED;
                        }
                        sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                        l_status_color.c_str(), l_off_color.c_str(),
                                        l_status_val_color, l_resp->get_status(), l_off_color.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Headers
                // TODO -only in json mode for now
                if(a_part_map & PART_HEADERS)
                {
                        // nuthin
                }

                // Body
                if(a_part_map & PART_BODY)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_rx)->m_response_body.length());
                        const char *l_body_buf = l_resp->get_body_data();
                        uint64_t l_body_len = l_resp->get_body_len();
                        if(l_body_len)
                        {
                                sprintf(l_buf, "\"%sbody%s\": %s",
                                                l_body_color.c_str(), l_off_color.c_str(),
                                                l_body_buf);
                        }
                        else
                        {
                                sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                l_body_color.c_str(), l_off_color.c_str());
                        }
                        ARESP(l_buf);
                        l_fbf = true;
                }
                ARESP("\n");
        }

        return l_responses_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define JS_ADD_MEMBER(_key, _val)\
l_obj.AddMember(_key,\
                rapidjson::Value(_val, l_js_allocator).Move(),\
                l_js_allocator)

std::string dump_all_responses_json(phurl_resp_list_t &a_resp_list, int a_part_map)
{
        rapidjson::Document l_js_doc;
        l_js_doc.SetObject();
        rapidjson::Value l_js_array(rapidjson::kArrayType);
        rapidjson::Document::AllocatorType& l_js_allocator = l_js_doc.GetAllocator();

        for(phurl_resp_list_t::const_iterator i_rx = a_resp_list.begin();
            i_rx != a_resp_list.end();
            ++i_rx)
        {

                ns_hlx::subr *l_subr = (*i_rx)->m_subr;
                ns_hlx::resp *l_resp = (*i_rx)->m_resp;
                if(!l_subr)
                {
                        continue;
                }

                rapidjson::Value l_obj;
                l_obj.SetObject();

                // Host
                if(a_part_map & PART_HOST)
                {
                        JS_ADD_MEMBER("host", l_subr->get_host().c_str());
                }

                // Server
                if(a_part_map & PART_SERVER)
                {
                        char l_server_buf[1024];
                        sprintf(l_server_buf, "%s:%d",
                                        l_subr->get_host().c_str(),
                                        l_subr->get_port());
                        JS_ADD_MEMBER("server", l_server_buf);

                        if(!l_subr->get_id().empty())
                        {
                                JS_ADD_MEMBER("id", l_subr->get_id().c_str());
                        }
                        if(!l_subr->get_where().empty())
                        {
                                JS_ADD_MEMBER("where", l_subr->get_where().c_str());
                        }
                        if(l_subr->get_port() != 0)
                        {
                                l_obj.AddMember("port", l_subr->get_port(), l_js_allocator);
                        }
                }

                if(!l_resp)
                {
                        l_js_array.PushBack(l_obj, l_js_allocator);
                        continue;
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        l_obj.AddMember("status-code", l_resp->get_status(), l_js_allocator);
                }

                // Headers
                const ns_hlx::kv_map_list_t &l_headers = l_resp->get_headers();
                if(a_part_map & PART_HEADERS)
                {
                        for(ns_hlx::kv_map_list_t::const_iterator i_header = l_headers.begin();
                            i_header != l_headers.end();
                            ++i_header)
                        {
                                for(ns_hlx::str_list_t::const_iterator i_val = i_header->second.begin();
                                    i_val != i_header->second.end();
                                    ++i_val)
                                {
                                        l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                        rapidjson::Value(i_val->c_str(), l_js_allocator).Move(),
                                                        l_js_allocator);
                                }
                        }
                }
                // ---------------------------------------------------
                // SSL connection info
                // ---------------------------------------------------
                // TODO Add flag -only add to output if flag
                // ---------------------------------------------------
                if((*i_rx)->m_tls_info_cipher_str)
                {
                        l_obj.AddMember(rapidjson::Value("Cipher", l_js_allocator).Move(),
                                        rapidjson::Value((*i_rx)->m_tls_info_cipher_str, l_js_allocator).Move(),
                                        l_js_allocator);
                }
                if((*i_rx)->m_tls_info_protocol_str)
                {
                        l_obj.AddMember(rapidjson::Value("Protocol", l_js_allocator).Move(),
                                        rapidjson::Value((*i_rx)->m_tls_info_protocol_str, l_js_allocator).Move(),
                                        l_js_allocator);
                }

                // Search for json
                bool l_content_type_json = false;
                ns_hlx::kv_map_list_t::const_iterator i_h = l_headers.find("Content-type");
                if(i_h != l_headers.end())
                {
                        for(ns_hlx::str_list_t::const_iterator i_val = i_h->second.begin();
                            i_val != i_h->second.end();
                            ++i_val)
                        {
                                if((*i_val) == "application/json")
                                {
                                        l_content_type_json = true;
                                }
                        }
                }

                // Body
                if(a_part_map & PART_BODY)
                {

                        //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_rx)->m_response_body.length());
                        const char *l_body_buf = l_resp->get_body_data();
                        uint64_t l_body_len = l_resp->get_body_len();
                        if(l_body_len)
                        {
                                // Append json
                                if(l_content_type_json)
                                {
                                        rapidjson::Document l_doc_body;
                                        l_doc_body.Parse(l_body_buf);
                                        l_obj.AddMember("body",
                                                        rapidjson::Value(l_doc_body, l_js_allocator).Move(),
                                                        l_js_allocator);

                                }
                                else
                                {
                                        JS_ADD_MEMBER("body", l_body_buf);
                                }
                        }
                        else
                        {
                                JS_ADD_MEMBER("body", "NO_RESPONSE");
                        }
                }

                l_js_array.PushBack(l_obj, l_js_allocator);

        }

        // TODO -Can I just create an array -do I have to stick in a document?
        l_js_doc.AddMember("array", l_js_array, l_js_allocator);
        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_js_writer(l_strbuf);
        l_js_doc["array"].Accept(l_js_writer);

        //NDBG_PRINT("Document: \n%s\n", l_strbuf.GetString());
        std::string l_responses_str = l_strbuf.GetString();

        return l_responses_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string dump_all_responses(phurl_resp_list_t &a_resp_list, bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
{
        std::string l_responses_str = "";
        switch(a_output_type)
        {
        case OUTPUT_LINE_DELIMITED:
        {
                l_responses_str = dump_all_responses_line_dl(a_resp_list, a_color, a_pretty, a_part_map);
                break;
        }
        case OUTPUT_JSON:
        {
                l_responses_str = dump_all_responses_json(a_resp_list, a_part_map);
                break;
        }
        default:
        {
                break;
        }
        }

        return l_responses_str;

}
