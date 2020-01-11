#include <iostream>
#define USE_MIPS 1
#if USE_MIPS
#include "/opt/libcurl-lib/include/curl/curl.h"
#else
#include <curl/curl.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include "cJSON.h"
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include<algorithm>
#define RESPONSE_DATA_LEN 4096
using namespace std;
//回调函数  得到响应内容
pthread_mutex_t mutex;
string g_server_port;
string g_server_ip;
string g_config_index;
FILE *g_fp_m3u8=NULL;
static vector<string> g_file_data_save;
char base[1000];
char mac[32] = {0};
std::vector<std::string> result;
DIR *dir=NULL;
char StrLine[100];
struct dirent *ptr=NULL;
string ts_root = "/tmp/hls/";
//用来接收服务器一个buffer
std::vector<string> Read_config(const char *path){
    FILE *fp =NULL;

}

typedef struct login_response_data
{
    login_response_data() {
        memset(data, 0, RESPONSE_DATA_LEN);
        data_len = 0;
    }

    char data[RESPONSE_DATA_LEN];
    int data_len;

}response_data_t;

size_t deal_response(void *ptr, size_t n, size_t m, void *arg)
{
    int count = m*n;

    response_data_t *response_data = (response_data_t*)arg;


    memcpy(response_data->data, ptr, count);

    response_data->data_len = count;

    return response_data->data_len;
}
int Get_cmd_data(string url,char *cmd,response_data_t &responseData);
int upload(string url, string &body,
                      response_data_t &responseData,
                      string imgpath, FILE *fp_m3u8,
                        long m3u8_size);
int get_request(string url, string &body, string &response);
string getmac()
{
#define MAXINTERFACES 16
    int fd, interface;
    struct ifreq buf[MAXINTERFACES];
    struct ifconf ifc;
    memset(mac,0,sizeof(mac)/sizeof(char));

    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        int i = 0;
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = (caddr_t)buf;
        if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
        {
            interface = ifc.ifc_len / sizeof(struct ifreq);
            printf("interface num is %d\n", interface);
            while (i < interface)
            {
                printf("net device %s\n", buf[i].ifr_name);
                if (!(ioctl(fd, SIOCGIFHWADDR, (char *)&buf[i])))
                {
                    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                            (unsigned char)buf[i].ifr_hwaddr.sa_data[0],
                            (unsigned char)buf[i].ifr_hwaddr.sa_data[1],
                            (unsigned char)buf[i].ifr_hwaddr.sa_data[2],
                            (unsigned char)buf[i].ifr_hwaddr.sa_data[3],
                            (unsigned char)buf[i].ifr_hwaddr.sa_data[4],
                            (unsigned char)buf[i].ifr_hwaddr.sa_data[5]);
//                    printf("HWaddr %s\n", mac);
                }
                printf("\n");
                i++;
            }
        }
    }
    printf("openwrt_mac=%s\n",mac);
    string net_mac=string(mac);
    return net_mac;
}


typedef  struct fileinfo_
{
    string filename;
    char *file_data;
}fileinfo;
//获取文件夹下的文件名
vector<string> Get_filename(const char *basePath)
{
    result.clear();
    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        exit(1);
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 8)    ///file
        {

            int index = std::string(ptr->d_name).find(".");
            if(index<0){
                continue;
            }
            string ts_name = std::string(ptr->d_name).substr(index);
            if((0==strcmp(ts_name.c_str(),".ts"))||(0==strcmp(ts_name.c_str(),".m3u8"))){
                result.push_back(std::string(ptr->d_name));
            }

        }
        else if(ptr->d_type == 10)    ///link file
        {printf("d_name:%s/%s\n",basePath,ptr->d_name);
            result.push_back(std::string(ptr->d_name));}
        else if(ptr->d_type == 4)    ///dir
        {
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            result.push_back(std::string(ptr->d_name));
            Get_filename(base);
        }
    }
    closedir(dir);
    return result;
}
struct MemoryStruct
{
    char *memory;
    size_t size;
    MemoryStruct()
    {
        memory = (char *)malloc(1);
        size = 0;
    }
    ~MemoryStruct()
    {
        free(memory);
        memory = NULL;
        size = 0;
    }
};


size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)data;

    mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory)
    {
        memcpy(&(mem->memory[mem->size]), ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }
    return realsize;
}
int write_data(void* ptr, int size, int nmemb, void* stream){
    string data((const char*) ptr, (size_t) size * nmemb);
    *((stringstream*) stream) << data << endl;
    return size * nmemb;
}



void *test_get_requse(void* args)
{

    char *path = (char*)args;
    string body,get_msg;
    string get_url="http://"+g_server_ip+":"+g_server_port+"/image_get?cmd=start_connect&type=string";
    int count =0;
    while (1)
    {
        count++;
        printf("test_get_requse,thread count run=%d\n",count);
        if (pthread_mutex_lock(&mutex) != 0){
            fprintf(stdout, "lock error!\n");
        }
        int status_code_ = get_request(get_url,
                body, get_msg);
        // 解锁
        pthread_mutex_unlock(&mutex);
        if (status_code_ != CURLcode::CURLE_OK) {
            cout<<"connect failed!\n";
            return NULL;
        }
        usleep(1000*100);

    }
}


void ReadLine(FILE *m3u8fp,vector<string> &file_data)
{
    vector<string> file_data_tmp;
    file_data_tmp.clear();
    int tmp_index=0;
    string temp;
    memset(StrLine,0, sizeof(StrLine));
    //将m3u8的ts列表读出
    while (fgets(StrLine, sizeof(StrLine), m3u8fp) != NULL)
    {
        StrLine[strlen(StrLine)-1] ='\0';
        string bufline =string(StrLine);
        file_data_tmp.push_back(bufline);
    }
    for(tmp_index=0;tmp_index<file_data_tmp.size();tmp_index++)
    {
        int index = file_data_tmp[tmp_index].find(".");
        if(index<0){
            continue;
        }
        string ts_name = file_data_tmp[tmp_index].substr(index);
        string ts_flag=".ts ";
        if(0==strcmp(ts_name.c_str(),".ts"))
        {
            file_data.push_back(file_data_tmp[tmp_index]);

        }
    }
}



int main(int argc, char** argv){

    printf("***ewrer****start to client ******eret***\n");
    printf("***ewrer****start to client ******eret***\n");
    if(argc!=4)
    {
        printf("param failed server_addr server_port config_index !\n");
        return  -1;
    }
    if (pthread_mutex_init(&mutex, NULL) != 0){
        // 互斥锁初始化失败
        return 1;
    }
    //set limit file num
    const char *cmd = "ulimit -n 102400";
    system(cmd);
    g_server_ip =argv[1];
    g_server_port=argv[2];
    g_config_index = argv[3];
    std::string body;
    std::string response;
    string get_msg;
    std::string img_path = argv[1];
    pthread_t th1,th2;
    void *thread_result;

    if(pthread_create(&th1,NULL,test_get_requse,(void*)img_path.c_str()) < 0)
    {
        perror("fail to pthread_create");
        exit(-1);
    }

    printf("waiting for thread to finish\n");
    if(pthread_join(th1,&thread_result) < 0)
    {
        perror("fail to pthread_join");
        exit(-1);
    }

    printf("waiting for thread to finish\n");


    return 0;
}


int get_filem3u8_size(FILE *fp)
{
    if(!fp) return -1;
    fseek(fp,0L,SEEK_END);
    int size=ftell(fp);
    fclose(fp);

    return size;
}

int get_request(string url, string &body, string &response)
{
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    if(CURLE_OK != res)
    {
        cout<<"curl init failed"<<endl;
        return 1;
    }
    std::stringstream out;

    CURL *pCurl = NULL;
    pCurl = curl_easy_init();
    static bool g_first_upload =false;
    response_data_t responseData;
    if( NULL == pCurl)
    {
        cout<<"Init CURL failed..."<<endl;
        return -1;
    }
    curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, 3L);//请求超时时长
    curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, 10L); //连接超时时长
//    curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);//允许重定向
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);  //得到请求结果后的回调函数

    MemoryStruct oDataChunk;  //请求结果的保存格式
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &oDataChunk);

//    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, deal_response);
//    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void *)&responseData);
//    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_data);
//    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &out);

    curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L); //关闭中断信号响应
    curl_easy_setopt(pCurl, CURLOPT_VERBOSE, 1L); //启用时会汇报所有的信息
    curl_easy_setopt(pCurl, CURLOPT_URL, url.c_str() ); //需要获取的URL地址

    curl_slist *pList = NULL;
    pList = curl_slist_append(pList,"Accept-Encoding:gzip, deflate, sdch");
    pList = curl_slist_append(pList,"Accept-Language:zh-CN,zh;q=0.8");
    pList = curl_slist_append(pList,"Connection:keep-alive");
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, pList);
    //执行请求
    res = curl_easy_perform(pCurl);
    string config_ts_index=g_config_index;
    long res_code=0;
    string post_url= "http://"+g_server_ip+":"+g_server_port+"/image_upload";
    res=curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &res_code);

    if(( res == CURLE_OK ) && (res_code == 200 || res_code == 201))
    {
        char* msg = oDataChunk.memory;
//        printf("recv msg = %s\n",msg);
        if(strcmp(msg,"upload_file")==0){
            // start post file to server
//            printf("start post file to server\n");



            string m3u8_name = "1.m3u8";

            //获取ts的文件名，
            vector<string> file_data;
            file_data.clear();
            FILE *fp_m3u8_tmp_size= NULL;
            fp_m3u8_tmp_size = fopen((config_ts_index+m3u8_name).c_str(),"r");
            int m3u8_size=get_filem3u8_size(fp_m3u8_tmp_size);
//            printf("m3u8_size=%d\n",m3u8_size);
            fp_m3u8_tmp_size = fopen((config_ts_index+m3u8_name).c_str(),"r");
            ReadLine(fp_m3u8_tmp_size,file_data);
            file_data.push_back(m3u8_name);//添加m3u8路径
            fclose(fp_m3u8_tmp_size);
            g_fp_m3u8 = fopen((config_ts_index+m3u8_name).c_str(),"r");
            //传入同一份文件流指针，
            if(g_fp_m3u8 == NULL) //判断文件是否存在及可读
            {
                printf("error!");
                return -1 ;
            }

            for(int i=0;i<file_data.size();i++){
                printf("index_ts_name = %s\n",(config_ts_index+file_data[i]).c_str());
                upload(post_url,body,responseData,(config_ts_index+file_data[i]).c_str(),
                       NULL,m3u8_size);
                printf("post_msg=%s\n", responseData.data);
                if(0!=strcmp(responseData.data,"postfile_ok")){
                    upload(post_url,body,responseData,(config_ts_index+file_data[i]).c_str(),
                           NULL,m3u8_size);
                }
            }

        }else {
            printf("start get cmd _data!\n");
           Get_cmd_data(post_url,msg,responseData);
            if(0!=strcmp(responseData.data,"postfile_ok")){
                Get_cmd_data(post_url,msg,responseData);
            }else{
                printf("run %s is successful!,then client exit\n",msg);
                exit(-1);
            }
        }
    }
    curl_slist_free_all(pList);
    curl_easy_cleanup(pCurl);
    curl_global_cleanup();
    return 0;


}

int get_file_size(const char* filename)
{
    FILE *fp=fopen(filename,"r");
    if(!fp) return -1;
    fseek(fp,0L,SEEK_END);
    int size=ftell(fp);
    fclose(fp);

    return size;
}
int executeCMD(string cmd,string &cmd_data) {
    FILE *fp = NULL;
    cmd =cmd + " 2>&1";
    char data[100] = {'0'};
    char result[1024] = {0};
    fp = popen(cmd.c_str(), "r");
    if (fp == NULL) {
        printf("popen error!\n");
        return 1;
    }
    while (fgets(data, sizeof(data), fp) != NULL) {
        strcat(result, data);
    }
    cmd_data = string(result);
    pclose(fp);
    return 0;
}
int Get_cmd_data(string url,char *cmd,response_data_t &responseData)
{

    char *send_file=NULL;
    CURL *curl=NULL;
    struct curl_slist *slist = NULL;
    CURLcode ret;
    static string net_mac;
    static bool get_mac= false;
    if(false ==get_mac){
        net_mac=getmac();
        replace(net_mac.begin(),net_mac.end(),':','_');
        printf("net_mac=%s\n",net_mac.c_str());
        get_mac= true;
    }

    string file_cmd_name="cmd_data.txt";
    string full_cmd = string(cmd) + " 2> " +file_cmd_name;
    printf("full_cmd = %s\n",full_cmd.c_str());
    system(full_cmd.c_str());
    long file_size=get_file_size(file_cmd_name.c_str());
    FILE * fp=NULL;

    fp =fopen(file_cmd_name.c_str(),"r");
    if(NULL ==fp){
        perror("create failed!\n");
        return -1;
    }
    send_file =(char*)malloc(file_size);
    if(NULL==send_file){
        fclose(fp);
        perror("malloc sendfile error!\n");
        return -1;
    }
    int readCount = fread(send_file, 1, file_size, fp);
    if (readCount != file_size) {
        fclose(fp);
        free(send_file);
        return -1;
    }


    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, (char *)url.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        curl_easy_setopt(curl, CURLOPT_POST, true);

        string filename_head ="filename:"+file_cmd_name;
        string net_flag ="net_flag:"+net_mac;
        slist = curl_slist_append(slist, "Content-Type: application/octet-stream");
        slist = curl_slist_append(slist,filename_head.c_str());
        slist = curl_slist_append(slist,net_flag.c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, file_size);//指定图片大小，否则遇到'\0'就停止了
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send_file);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, deal_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&responseData);        //绑定响应内容的地址

        ret = curl_easy_perform(curl);                          //执行请求
        remove(file_cmd_name.c_str());
        if(ret == 0){
            curl_easy_cleanup(curl);
            free(send_file);
            return 0;
        }
        else{
            free(send_file);
            return ret;
        }
    }else{
        free(send_file);
        return 0;
    }

}
int upload(string url, string &body,
        response_data_t &responseData,
        string imgpath, FILE *fp_m3u8,long m3u8_size)
{

//    response_data_t responseData;

    CURL *curl;
    CURLcode ret;
    curl = curl_easy_init();
    struct curl_httppost* post = NULL;
    struct curl_httppost* last = NULL;
    struct curl_slist *slist = NULL;
    string filename;
    FILE* fp =NULL;
    char *send_file=NULL;
    long  file_size=0;
    int index = imgpath.find(".");

    if(index<0){

    }else{
        string ts_name = imgpath.substr(index);
        if(0==strcmp(".ts",ts_name.c_str())){
            int pos = imgpath.find_last_of('/');
            filename= imgpath.substr(pos + 1);
            fp=fopen(imgpath.c_str(),"r");
            if(NULL==fp){

                perror("open file error!\n");
                return -1;
            }

            file_size=get_file_size(imgpath.c_str());
//            printf("file_size=%d\n",file_size);
//            printf("file_name=%s\n",filename.c_str());
            send_file=(char*)malloc(file_size);
            if(0==file_size){
                printf("malloc file_size is zero!\n");
                free(send_file);
                return -1;
            }
            if(NULL==send_file){
                printf("malloc file_size memory faied!\n");
                return -1;
            }

            int readCount = fread(send_file, 1, file_size, fp);
            if (readCount != file_size) {
                fclose(fp);
                free(send_file);
                return -1;
            }
        }else{

            file_size = m3u8_size;

            int pos = imgpath.find_last_of('/');
            filename= imgpath.substr(pos + 1);
//            printf("file_size=%d\n",file_size);
//            printf("file_name=%s\n",filename.c_str());
            send_file=(char*)malloc(file_size);
            if(0==file_size){
                printf("malloc file_size is zero!\n");
                free(send_file);
                return -1;
            }
            if(NULL==send_file){
                printf("malloc file_size memory faied!\n");
                return -1;
            }

            int readCount = fread(send_file, 1, file_size, g_fp_m3u8);
            if (readCount != file_size) {
                fclose(fp);
                free(send_file);
                return -1;
            }

        }
    }

    static string net_mac;
    static bool get_mac= false;
    if(false ==get_mac){
        net_mac=getmac();
        replace(net_mac.begin(),net_mac.end(),':','_');
        printf("net_mac=%s\n",net_mac.c_str());
        get_mac= true;
    }

    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, (char *)url.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        curl_easy_setopt(curl, CURLOPT_POST, true);

        string filename_head ="filename:"+filename;
        string net_flag ="net_flag:"+net_mac;
        slist = curl_slist_append(slist, "Content-Type: application/octet-stream");
        slist = curl_slist_append(slist,filename_head.c_str());
        slist = curl_slist_append(slist,net_flag.c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, file_size);//指定图片大小，否则遇到'\0'就停止了
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send_file);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, deal_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&responseData);        //绑定响应内容的地址

        ret = curl_easy_perform(curl);                          //执行请求
        if(ret == 0){
            curl_easy_cleanup(curl);
            free(send_file);
            return 0;
        }
        else{
            free(send_file);
            return ret;
        }
    }else{
        free(send_file);
        return 0;
    }
}