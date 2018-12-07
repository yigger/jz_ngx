#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <curl/curl.h>
#include "cJSON.h"

#define  AUTHORIZE_OK          0
#define  AUTHORIZE_FAIL       -1

typedef struct {
    ngx_str_t output_words;
} ngx_http_private_image_loc_conf_t;

static char* ngx_http_private_image(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
 
static void* ngx_http_private_image_create_loc_conf(ngx_conf_t* cf);
 
static char* ngx_http_private_image_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);

static ngx_int_t ngx_http_private_image_handler(ngx_http_request_t* r);

static ngx_str_t get_key_header (ngx_http_request_t* r, ngx_str_t header_name);

static ngx_int_t check_authorize(ngx_http_request_t* r, char *header);

static ngx_command_t ngx_http_private_image_commands[] = {
    {
        // 配置指令的名称。
        ngx_string("private_image"),
        // 该配置的类型，其实更准确一点说，是该配置指令属性的集合。nginx提供了很多预定义的属性值（一些宏定义），通过逻辑或运算符可组合在一起，形成对这个配置指令的详细的说明。下面列出可在这里使用的预定义属性值及说明。
        NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
        // 这是一个函数指针，当nginx在解析配置的时候，如果遇到这个配置指令，将会把读取到的值传递给这个函数进行分解处理。因为具体每个配置指令的值如何处理，只有定义这个配置指令的人是最清楚的。来看一下这个函数指针要求的函数原型。
        ngx_http_private_image,
        // 该字段被NGX_HTTP_MODULE类型模块所用 (我们编写的基本上都是NGX_HTTP_MOUDLE，只有一些nginx核心模块是非NGX_HTTP_MODULE)，该字段指定当前配置项存储的内存位置。实际上是使用哪个内存池的问题。因为http模块对所有http模块所要保存的配置信息，划分了main, server和location三个地方进行存储，每个地方都有一个内存池用来分配存储这些信息的内存。这里可能的值为 NGX_HTTP_MAIN_CONF_OFFSET、NGX_HTTP_SRV_CONF_OFFSET或NGX_HTTP_LOC_CONF_OFFSET。当然也可以直接置为0，就是NGX_HTTP_MAIN_CONF_OFFSET。
        NGX_HTTP_LOC_CONF_OFFSET,
        // 指定该配置项值的精确存放位置，一般指定为某一个结构体变量的字段偏移。因为对于配置信息的存储，一般我们都是定义个结构体来存储的。那么比如我们定义了一个结构体A，该项配置的值需要存储到该结构体的b字段。那么在这里就可以填写为offsetof(A, b)。对于有些配置项，它的值不需要保存或者是需要保存到更为复杂的结构中时，这里可以设置为0。
        offsetof(ngx_http_private_image_loc_conf_t, output_words),
        // 该字段存储一个指针。可以指向任何一个在读取配置过程中需要的数据，以便于进行配置读取的处理。大多数时候，都不需要，所以简单地设为0即可。
        NULL
    },
    // 需要注意的是，就是在ngx_http_hello_commands这个数组定义的最后，都要加一个ngx_null_command作为结尾。
    ngx_null_command
};

static ngx_http_module_t ngx_http_private_image_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_private_image_create_loc_conf,
    ngx_http_private_image_merge_loc_conf
};

ngx_module_t ngx_http_private_image_module  = {
    NGX_MODULE_V1,
    &ngx_http_private_image_module_ctx,
    ngx_http_private_image_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static char 
*ngx_http_private_image(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_private_image_handler;
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_private_image_handler(ngx_http_request_t* r)
{
    ngx_log_t                 *log;
    ngx_int_t                  rc;
    u_char                    *last;
    size_t                     root;
    ngx_str_t                  path;
    ngx_chain_t                out;
    ngx_buf_t                 *b;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_open_file_info_t       of;

    // 只允许 get head 请求
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // 请求最后 / 说明不是文件 拒绝响应
    if (r->uri.data[r->uri.len - 1] == '/') {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // 请求参数 HEDAER
    ngx_str_t header_key = ngx_string("WX-KEY");
    ngx_str_t header_val = get_key_header(r, header_key);
    if (header_val.data == NULL) {
        return NGX_HTTP_FORBIDDEN;
    } else {
        // 字符串拼接
        char *header;
        ngx_int_t new_len = header_key.len + header_val.len + 2;
        header = malloc(new_len);
        ngx_memcpy(header, header_key.data, header_key.len);
        strcat(header, ":");
        strcat(header, (char *)header_val.data);

        // 进行权限校验
        if (check_authorize(r, header) == AUTHORIZE_FAIL) {
            free(header);
            return NGX_HTTP_FORBIDDEN;
        }

        free(header);
    }

    // 转换为磁盘路径 path
    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (last == NULL) {
        return NGX_HTTP_NOT_FOUND;
    }

    // 初始化 Log
    log = r->connection->log;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool) != NGX_OK)
    {
        ngx_log_error(NGX_LOG_ERR, log, of.err, "%s \"%s\" private image failed", of.failed, path.data);
    }

    ngx_str_t type = ngx_string("image/png");
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = of.size;
    r->headers_out.last_modified_time = of.mtime;
    r->headers_out.content_type = type;
    r->allow_ranges = 1;

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    b->file_pos = 0;
    b->file_last = of.size;

    b->in_file = b->file_last ? 1: 0;
    b->last_buf = (r == r->main) ? 1: 0;
    b->last_in_chain = 1;

    b->file->fd = of.fd;
    b->file->name = path;
    b->file->log = log;
    b->file->directio = of.is_directio;
    
    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}

static void*
ngx_http_private_image_create_loc_conf(ngx_conf_t* cf)
{
    ngx_http_private_image_loc_conf_t* conf;
 
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_private_image_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    conf->output_words.len = 0;
    conf->output_words.data = NULL;
 
    return conf;
}
 
static char*
ngx_http_private_image_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child) 
{
    ngx_http_private_image_loc_conf_t* prev = parent;
    ngx_http_private_image_loc_conf_t* conf = child;
    ngx_conf_merge_str_value(conf->output_words, prev->output_words, "Nginx");
    return NGX_CONF_OK;
}

size_t
getResponse(void* ptr, size_t size, size_t nmemb, ngx_str_t *response) // struct string *s
{
    size_t new_len = response->len + size*nmemb;
    response->data = realloc(response->data, new_len + 1);
    if (response->data == NULL) {
        return NGX_ERROR;
    }
    ngx_memcpy(response->data, ptr, size*nmemb);
    response->data[new_len] = '\0';
    response->len = new_len;

    return size*nmemb;
}

static ngx_int_t
check_authorize(ngx_http_request_t* r, char *header)
{
    ngx_int_t          result   = AUTHORIZE_FAIL;
    ngx_str_t          response = ngx_null_string;
    CURLcode           curl_code;
    CURL              *curl;
    struct curl_slist *chunk    = NULL;

    char prefix[] = "source_url=";
    char *post_field = malloc(strlen(prefix)+r->uri.len+200);    
    strcpy(post_field, prefix);
    strcat(post_field, (char *)r->uri.data);

    curl = curl_easy_init();
    if (curl) {
        // set request url and set response
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:1323");
 
        /* Now specify we want to POST data */ 
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field);

        /* we want to use our own read function */ 
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, getResponse);
    
        /* pointer to pass to our read function */ 
        curl_easy_setopt(curl, CURLOPT_READDATA, &response);
    
        /* get verbose debug output please */ 
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // set request headers
        chunk = curl_slist_append(chunk, header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OK && response.data != NULL) {
            // get response json and check
            cJSON* parse = cJSON_Parse((char *)response.data);
            cJSON* status = cJSON_GetObjectItem(parse, "status");
            if (status->valuestring != NULL && ngx_strcmp(status->valuestring, "200") == 0) {
                result = AUTHORIZE_OK;
            }
            cJSON_free(parse);
            cJSON_free(status);
        }

        free(post_field);
        curl_easy_cleanup(curl);
        curl_slist_free_all(chunk);
    }

    return result;
}

static ngx_str_t
get_key_header (ngx_http_request_t* r, ngx_str_t header_name) {
    ngx_list_part_t *part = &r->headers_in.headers.part;
    ngx_table_elt_t *header = part->elts;
    ngx_str_t result = ngx_null_string;
    unsigned int i = 0;

    for(;;i ++) {
        if (i >= part->nelts) {
            if ((part->next == NULL)) {
                break;
            }
            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].hash == 0) {
            continue;
        }

        if (0 == ngx_strncasecmp(header[i].key.data, header_name.data, header[i].key.len)) {
            return header[i].value;
        }
    }

    return result;
}
