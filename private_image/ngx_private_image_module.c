#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static ngx_int_t ngx_http_private_image_handle(ngx_http_request_t *r);
static ngx_int_t ngx_private_image_init(ngx_conf_t *cf);

typedef struct {
  ngx_str_t output_words;
} ngx_http_private_image_loc_conf_t;

static ngx_command_t ngx_http_private_image_commands[] = {
  {
    // 配置指令的名称。
    ngx_string("private_image"),
    // 该配置的类型，其实更准确一点说，是该配置指令属性的集合。nginx提供了很多预定义的属性值（一些宏定义），通过逻辑或运算符可组合在一起，形成对这个配置指令的详细的说明。下面列出可在这里使用的预定义属性值及说明。
    NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
    // 这是一个函数指针，当nginx在解析配置的时候，如果遇到这个配置指令，将会把读取到的值传递给这个函数进行分解处理。因为具体每个配置指令的值如何处理，只有定义这个配置指令的人是最清楚的。来看一下这个函数指针要求的函数原型。
    NULL,
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

static ngx_http_module_t ngx_http_private_image_ctx = {
	NULL,
	ngx_private_image_init,				   /* postconfiguration */
	NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */
    NULL,                                  /* create location configuration */
    NULL  
};

ngx_module_t ngx_http_private_image_module = {
	NGX_MODULE_V1,
	&ngx_http_private_image_ctx,
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

static ngx_int_t
ngx_http_private_image_handle(ngx_http_request_t *r)
{
	ngx_str_t 		path;
	size_t 			root;
	u_char 			*last;


	// 仅允许 GET/HEAD 请求
	if (!(r->method) & (NGX_HTTP_GET|NGX_HTTP_HEAD)) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	// 以 / 结尾， 说明不是文件，交由后续模块处理
	if (r->uri.data[r->uri.len - 1] == '/') {
		return NGX_DECLINED;
	}

	last = ngx_http_map_uri_to_path(r, &path, &root, 0);
	if (last == NULL) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	

}

static ngx_int_t
ngx_private_image_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_http_private_image_handle;
	return NGX_OK;
}