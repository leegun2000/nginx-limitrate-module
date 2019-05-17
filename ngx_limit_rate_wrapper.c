#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct ngx_http_limit_rate_conf_s {
        ngx_int_t       limit_rate_index;
        ngx_int_t       limit_rate_after_index;
        size_t          limit_rate;
        size_t          limit_rate_after;
} ngx_http_limit_rate_conf_t;

static void *ngx_http_limit_rate_create_loc_conf(ngx_conf_t *cf);

static ngx_int_t ngx_http_limit_rate_init(ngx_conf_t *cf);
static void *ngx_http_limit_rate_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_limit_rate_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_limit_rate(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_limit_rate_header_filter(ngx_http_request_t *r);

#define LIMIT_RATE_VAR                  "limit_rate_var"
#define LIMIT_RATE_AFTER_VAR            "limit_rate_after_var"
#define LIMIT_RATE_VAR_LEN              (sizeof(LIMIT_RATE_VAR) - 1)
#define LIMIT_RATE_AFTER_VAR_LEN        (sizeof(LIMIT_RATE_AFTER_VAR) - 1)

static ngx_command_t  ngx_http_limit_rate_commands[] = {

    {
        ngx_string(LIMIT_RATE_AFTER_VAR),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_limit_rate,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    {
        ngx_string(LIMIT_RATE_VAR),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_limit_rate,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t  ngx_http_limit_rate_filter_module_ctx = {
    NULL,                                       /* preconfiguration */
    ngx_http_limit_rate_init,                   /* postconfiguration */

    NULL,                                       /* create main configuration */
    NULL,                                       /* init main configuration */

    NULL,                                       /* create server configuration */
    NULL,                                       /* merge server configuration */

    ngx_http_limit_rate_create_loc_conf,        /* create location configuration */
    ngx_http_limit_rate_merge_loc_conf          /* merge location configuration */
};

ngx_module_t  ngx_http_limit_rate_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_limit_rate_filter_module_ctx, /* module context */
    ngx_http_limit_rate_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static void *
ngx_http_limit_rate_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_limit_rate_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_limit_rate_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->limit_rate_index = NGX_CONF_UNSET;
    conf->limit_rate_after_index = NGX_CONF_UNSET;
    conf->limit_rate = NGX_CONF_UNSET_SIZE;
    conf->limit_rate_after = NGX_CONF_UNSET_SIZE;
    return conf;
}

static char *
ngx_http_limit_rate_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_limit_rate_conf_t *prev = parent;
    ngx_http_limit_rate_conf_t *conf = child;

    ngx_conf_merge_size_value(conf->limit_rate, prev->limit_rate, NGX_CONF_UNSET_SIZE);
    ngx_conf_merge_size_value(conf->limit_rate_after, prev->limit_rate_after, NGX_CONF_UNSET_SIZE);
    ngx_conf_merge_value(conf->limit_rate_index, prev->limit_rate_index, NGX_CONF_UNSET);
    ngx_conf_merge_value(conf->limit_rate_after_index, prev->limit_rate_after_index, NGX_CONF_UNSET);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_limit_rate_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_limit_rate_header_filter;

    return NGX_OK;
}

static ngx_int_t
ngx_http_limit_rate_header_filter(ngx_http_request_t *r)
{
    ngx_http_limit_rate_conf_t      *lcf;
    ngx_http_variable_value_t *var;
    ngx_str_t str;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_limit_rate_filter_module);
    if (!lcf) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "%s, unable to get module's config context, call next filter", __func__);
        return ngx_http_next_header_filter(r);
    }

    /* get index value */
    if (lcf->limit_rate_index != NGX_CONF_UNSET) {
        var = ngx_http_get_indexed_variable(r, lcf->limit_rate_index);
        if (var == NULL || var->len == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                    "%s, limit_rate failed get variable", __func__);
            return NGX_ERROR;
        }
        str.data = var->data;
        str.len = var->len;
        lcf->limit_rate = ngx_parse_size(&str);
    }

    if (lcf->limit_rate_after_index != NGX_CONF_UNSET) {
        var = ngx_http_get_indexed_variable(r, lcf->limit_rate_after_index);
        if (var == NULL || var->len == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                      "%s, limit_rate_after failed get variable", __func__);
            return NGX_ERROR;
        }
            
        str.data = var->data;
        str.len = var->len;
        lcf->limit_rate_after = ngx_parse_size(&str);
    }
        
    if (lcf->limit_rate != NGX_CONF_UNSET_SIZE) {
        r->limit_rate = lcf->limit_rate;
    }

    if (lcf->limit_rate_after != NGX_CONF_UNSET_SIZE) {        
        r->limit_rate_after = lcf->limit_rate_after;
    }

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "%s, limit_rate: %z, limit_rate_after: %z", __func__, lcf->limit_rate, lcf->limit_rate_after);

    return ngx_http_next_header_filter(r);
}

static char *
ngx_http_limit_rate(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_limit_rate_conf_t *lcf = conf;
    ngx_int_t index;
    ngx_str_t *value;
    ngx_flag_t limit_rate = 0, limit_rate_after = 0;
    ssize_t sp;

    value = cf->args->elts;

    if (value[0].len == LIMIT_RATE_VAR_LEN
        && !ngx_strncmp(value[0].data, LIMIT_RATE_VAR, LIMIT_RATE_VAR_LEN)) {
        limit_rate = 1;                
    } else if (value[0].len == LIMIT_RATE_AFTER_VAR_LEN
               && !ngx_strncmp(value[0].data, LIMIT_RATE_AFTER_VAR, LIMIT_RATE_AFTER_VAR_LEN)) {
        limit_rate_after = 1;
    }

    if (value[1].data[0] != '$') {
        sp = ngx_parse_size(&value[1]);
        if (sp != (ssize_t) NGX_ERROR) {
            if (limit_rate) {
                lcf->limit_rate = sp;
            } else if (limit_rate_after) {
                lcf->limit_rate_after = sp;
            }
            return NGX_CONF_OK;
        }
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "%s, invalid value \"%V\"", __func__, &value[1]);

        return NGX_CONF_ERROR;
    }

    value[1].len--;
    value[1].data++;

    index = ngx_http_get_variable_index(cf, &value[1]);
    if (index == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    if (limit_rate) {
        lcf->limit_rate_index = index;
    } else if (limit_rate_after) {
        lcf->limit_rate_after_index = index;
    }

    return NGX_CONF_OK;
}
