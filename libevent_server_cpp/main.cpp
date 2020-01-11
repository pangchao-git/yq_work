#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <evhttp.h>
#include <event.h>
#include <string.h>
#include <string>
#include "event2/http.h"
#include "event2/event.h"
#include "event2/buffer.h"
#include "event2/bufferevent.h"
#include "event2/bufferevent_compat.h"
#include "event2/http_struct.h"
#include "event2/http_compat.h"
#include "event2/util.h"
#include "event2/listener.h"
#include "cJSON.h"
#include <unistd.h>
using namespace std;
char file_name[100] ={0};
char net_mac[100] ={0};
char response_data[20]={0} ;
#define BUF_MAX 1024*16
#define MYHTTPD_SIGNATURE   "MoCarHttpd v0.1"
//解析post请求数据
char *find_http_header(struct evhttp_request *req,struct evkeyvalq *params,const char *query_char);
void get_post_message(struct evhttp_request *req)
{
    memset(file_name,0, sizeof(file_name)/sizeof(char));
    memset(net_mac,0, sizeof(net_mac)/sizeof(char));
    memset(response_data,0, sizeof(response_data)/sizeof(char));
    struct evkeyvalq *headers;
    struct evkeyval *header;
    struct evbuffer *evb = NULL;
    FILE *fp=NULL;
    const char *uri = evhttp_request_get_uri (req);
    struct evhttp_uri *decoded = NULL;

    char *request_data_buf=NULL;

    headers = evhttp_request_get_input_headers(req);
    int i=0;
    for (header = headers->tqh_first; header; header = header->next.tqe_next)
    {
//        cout << "  " << header->key <<  ": " << header->value << endl;
//        printf("header->key=%s,header->value=%s\n",header->key,header->value);
        if(i==3){
            memcpy(file_name ,header->value,strlen(header->value));
        }
        if(i==4){
            memcpy(net_mac ,header->value,strlen(header->value));
        }
        i++;
    }

    struct evkeyvalq sign_params = {0};
    char reply[100]={0};
//    printf("test===%s\n",header[3].value);
    //获取post请求uri中的filename参数
//    printf ("Got a POST request for filename:<%s>\n", file_name);
//    printf ("Got a POST request for net_mac:<%s>\n", net_mac);

    //判断此URI是否合法
    decoded = evhttp_uri_parse (uri);
    if (! decoded)
    {
        printf ("It's not a good URI. Sending BADREQUEST\n");
        evhttp_send_error (req, HTTP_BADREQUEST, 0);
        return;
    }
    struct evbuffer *buf_tmp = evhttp_request_get_input_buffer (req);
//    evbuffer_add (buf_tmp, "", 1);    /* NUL-terminate the buffer */
    char *payload = (char *) evbuffer_pullup (buf_tmp, -1);

    int post_data_len = evbuffer_get_length(buf_tmp);

    if(post_data_len<0){
        printf("upload_failed ,send masg to client\n");
        memset(response_data,0,strlen(response_data));
        memcpy(response_data,"postfile_failed",strlen("postfile_failed"));
    }else{
        request_data_buf = (char*)malloc(post_data_len);
        if(NULL==request_data_buf){
            perror("malloc error!\n");
            return ;
        }else{
//            printf("upload_ok ,send msg to client\n");
            memset(response_data,0, sizeof(response_data));
            char *reply ="postfile_ok";
            memcpy(response_data,reply,strlen(reply));
        }
    }

    memcpy(request_data_buf, payload, post_data_len);
    string  root_ts=string(net_mac);
    if(access(root_ts.c_str(),0)!=0){
        int ret = mkdir(root_ts.c_str(),0777);
        if(ret!=0){
            perror("mkdir failed!\n");
            return ;
        }
    }
    //保存文件
    printf("file_name=%s\n", file_name);
    string file_fullpath = root_ts+"/"+string(file_name);
    //判断当前的m3u8列表下面的ts是否上传完毕，所有ts上传完毕再上传ts
    //获取后缀名
    int index = file_fullpath.find(".");
    if(index<0){
//        continue;
    }//判断文件是否存在
    string ts_name = file_fullpath.substr(index);
    if(0==strcmp(".ts",ts_name.c_str())){
        if(!access(file_fullpath.c_str(),0)){
            usleep(100*10);
            fp=fopen(file_fullpath.c_str(),"w");
            if(NULL==fp){
                perror("fopen!");
                return ;
            }
            fwrite(request_data_buf,post_data_len,1,fp);
        }else{
            printf("%s is not exist\n",file_fullpath.c_str());
            fp=fopen(file_fullpath.c_str(),"w");
            if(NULL==fp){
                perror("fopen!");
                return ;
            }
            fwrite(request_data_buf,post_data_len,1,fp);

        }
    }else if(0==strcmp(".m3u8",ts_name.c_str())){
        usleep(100*10);
        fp=fopen(file_fullpath.c_str(),"w");
        if(NULL==fp){
            perror("fopen!");
            return ;
        }
        fwrite(request_data_buf,post_data_len,1,fp);

    }else if (0==strcmp(".txt",ts_name.c_str())){
        fp=fopen(file_fullpath.c_str(),"w");
        if(NULL==fp){
            perror("fopen!");
            return ;
        }
        fwrite(request_data_buf,post_data_len,1,fp);
    }else{
        printf("another txt!\n");
    }


    evb = evbuffer_new ();
    printf("response_data=%s\n",response_data);
    evbuffer_add_printf(evb, response_data);
    //将封装好的evbuffer 发送给客户端
    evhttp_send_reply(req, HTTP_OK, "OK", evb);

    if (decoded)
        evhttp_uri_free (decoded);
    if (evb)
        evbuffer_free (evb);

    fclose(fp);
    free(request_data_buf);
}

//解析http头，主要用于get请求时解析uri和请求参数
char *find_http_header(struct evhttp_request *req,struct evkeyvalq *params,const char *query_char)
{
    if(req == NULL || params == NULL || query_char == NULL)
    {
        printf("====line:%d,%s\n",__LINE__,"input params is null.");
        return NULL;
    }

    struct evhttp_uri *decoded = NULL;
    char *query = NULL;
    char *query_result = NULL;
    const char *path;
    const char *uri = evhttp_request_get_uri(req);//获取请求uri

    if(uri == NULL)
    {
        printf("====line:%d,evhttp_request_get_uri return null\n",__LINE__);
        return NULL;
    }
    else
    {
        printf("====line:%d,Got a GET request for <%s>\n",__LINE__,uri);
    }

    //解码uri
    decoded = evhttp_uri_parse(uri);
    if (!decoded)
    {
        printf("====line:%d,It's not a good URI. Sending BADREQUEST\n",__LINE__);
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return "-1";
    }

    //获取uri中的path部分
    path = evhttp_uri_get_path(decoded);
    if (path == NULL)
    {
        path = "/";
    }
    else
    {
        printf("====line:%d,path is:%s\n",__LINE__,path);
    }

    //获取uri中的参数部分
    query = (char*)evhttp_uri_get_query(decoded);
    if(query == NULL)
    {
        printf("====line:%d,evhttp_uri_get_query return null\n",__LINE__);
        return NULL;
    }

    //查询指定参数的值
    evhttp_parse_query_str(query, params);
    query_result = (char*)evhttp_find_header(params, query_char);

    return query_result;
}

void Read_config(const char *path,char*buffer){

#define MAX_LINE 100
    FILE *fp;            /*文件指针*/
    int len;             /*行字符个数*/
    if((fp = fopen(path,"r")) == NULL)
    {
        perror("fail to read");
        exit (1) ;
    }
    while(fgets(buffer,MAX_LINE,fp) != NULL)
    {
        len = strlen(buffer);
        buffer[len-1] = '\0';  /*去掉换行符*/
    }
}

//处理get请求
void http_handler_testget_msg(struct evhttp_request *req,void *arg)
{
    const char * config_file ="./config.ini";
    if(req == NULL)
    {
        printf("====line:%d,%s\n",__LINE__,"input param req is null.");
        return;
    }

    char *sign = NULL;
    char *data = NULL;
    struct evkeyvalq sign_params = {0};
    char reply[100]={0};
    data = find_http_header(req,&sign_params,"cmd");//获取get请求uri中的data参数
    if(data == NULL)
    {
        printf("====line:%d,%s\n",__LINE__,"request uri no param data.");
    }
    else
    {
        if(strcmp(data ,"start_connect")==0){
            Read_config(config_file,reply);
            printf("get_reply=%s\n",reply);
        }else{
            memcpy(reply,"another_cmd", sizeof("another_cmd"));
        }
    }

    //回响应
    struct evbuffer *retbuff = NULL;
    retbuff = evbuffer_new();
    if(retbuff == NULL)
    {
        printf("====line:%d,%s\n",__LINE__,"retbuff is null.");
        return;
    }
    //set msg
    evbuffer_add_printf(retbuff,reply);
    evhttp_send_reply(req,HTTP_OK,"Client",retbuff);
    evbuffer_free(retbuff);
}

//处理post请求
void http_handler_testpost_msg(struct evhttp_request *req,void *arg)
{
    if(req == NULL)
    {
        printf("====line:%d,%s\n",__LINE__,"input param req is null.");
        return;
    }
    get_post_message(req);//获取请求数据，一般是json格式的数据
}

int main()
{
    printf("server start to listening...\n");
    printf("server start to listening...\n");
    struct evhttp *http_server = NULL;
    short http_port = 8082;
    char *http_addr = "0.0.0.0";

    const char *cmd = "ulimit -n 102400";
    system(cmd);

    //初始化
    event_init();
    //启动http服务端
    http_server = evhttp_start(http_addr,http_port);
    if(http_server == NULL)
    {
        printf("====line:%d,%s\n",__LINE__,"http server start failed.");
        return -1;
    }

    //设置请求超时时间(s)
    evhttp_set_timeout(http_server,5);
    //设置事件处理函数，evhttp_set_cb针对每一个事件(请求)注册一个处理函数，
    //区别于evhttp_set_gencb函数，是对所有请求设置一个统一的处理函数
    evhttp_set_cb(http_server,"/image_upload",http_handler_testpost_msg,NULL);
    evhttp_set_cb(http_server,"/image_get",http_handler_testget_msg,NULL);

    //循环监听
    event_dispatch();
    //实际上不会释放，代码不会运行到这一步
    evhttp_free(http_server);

    return 0;
}
