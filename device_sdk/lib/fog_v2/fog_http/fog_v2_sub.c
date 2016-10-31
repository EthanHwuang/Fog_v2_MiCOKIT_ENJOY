#include "fog_v2_config.h"
#include "fog_v2_sub.h"
#include "mico.h"
#include "fog_v2_user_notification.h"

#define app_log(M, ...)                     custom_log("FOG_V2_SUB", M, ##__VA_ARGS__)

#ifndef FOG_V2_USE_SUB_DEVICE
    #error "FOG_V2_USE_SUB_DEVICE is not define"
#endif

#if (FOG_V2_USE_SUB_DEVICE == 1)

OSStatus fog_v2_subdevice_des_init(void);

static OSStatus fog_v2_subdevice_register(const char *product_id, const char *mac);
static OSStatus fog_v2_subdevice_unregister(const char *s_product_id, const char *s_mac);
static OSStatus fog_v2_subdevice_attach(const char *s_product_id, const char *s_mac);
static OSStatus fog_v2_subdevice_detach(const char *s_product_id, const char *s_mac);
static OSStatus fog_v2_subdevice_send_event(const char *payload, const char *s_product_id, const char *s_mac, uint32_t type);

//static OSStatus add_mqtt_topic_cmd_by_mac(const char *s_product_id, const char *mac);
OSStatus remove_mqtt_topic_by_mac(const char *s_product_id, const char *mac);
OSStatus add_mqtt_topic_by_mac(const char *s_product_id, const char *mac);
static OSStatus add_mqtt_topic_command_by_mac(const char *s_product_id, const char *s_mac);

OSStatus fog_v2_subdevice_add_timeout(const char *s_product_id);

OSStatus fog_v2_add_subdevice( const char *s_product_id, const char *s_mac, bool auto_set_online);
OSStatus fog_v2_remove_subdevice( const char *s_product_id, const char *s_mac );
OSStatus fog_v2_set_subdevice_status(const char *s_product_id, const char *s_mac, bool online);
OSStatus fog_v2_subdevice_send(const char *s_product_id, const char *s_mac, const char *payload, uint32_t flag);
OSStatus fog_v2_subdevice_recv(const char *s_product_id, const char *s_mac, char *payload, uint32_t payload_len, uint32_t timeout);
OSStatus fog_v2_subdeice_get_list(char *http_response, uint32_t recv_len, bool *get_http_response_success);

//bind monitor�������
void start_gateway_bind_monitor(void);
bool is_subdevice_cmd_queue_init(void);//�����Ƿ��Ѿ���ʼ��
OSStatus push_cmd_to_subdevice_queue(SUBDEVICE_CMD_TYPE type, const char *device_id);//�������в���һ������

static mico_queue_t sub_device_cmd_queue = NULL; //�������豸cmd��Ϣ�Ķ���

//���豸�豸ע��
static OSStatus fog_v2_subdevice_register(const char *s_product_id, const char *s_mac)
{
    OSStatus err = kGeneralErr;
    const char *sub_device_register_body = "{\"productid\":\"%s\",\"mac\":\"%s\",\"extend\":\"%s\"}";
    char http_body[256] = {0};
    char device_id_temp[64] = {0};
    int32_t code = -1;
    uint32_t index = 0;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;

    if ( fog_v2_is_have_superuser( ) == false )
    {
        app_log("[ERROR]gateway don't have superuser!");
        return kGeneralErr;
    }

    if(get_sub_device_queue_index_by_mac(&index, s_product_id, s_mac) == true)
    {
        app_log("this device is already register!!! index = %ld", index);
        return kNoErr;
    }

start_subdevice_register:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdevice_register is waitting...");
        mico_thread_msleep(200);
    }

    sprintf(http_body, sub_device_register_body, s_product_id, s_mac, "123");

    app_log("=====> sub_device_register send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_REGISTER_METHOD, FOG_V2_SUB_REGISTER_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    //�������ؽ��
    err = process_response_body_string(user_http_res.fog_response_body, &code, "deviceid", device_id_temp, sizeof(device_id_temp));
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    require_noerr( err, exit );

    if(sub_device_queue_get(s_product_id, s_mac, device_id_temp) == false)
    {
        app_log("sub_device_queue_get() error!!!");
        return kGeneralErr;
    }

    app_log("register success, sub_device_id: %s", device_id_temp);
    app_log("<===== sub_device_register success <======");

 exit:
    if(err != kNoErr)
    {
        if ( (code == FOG_HTTP_PRODUCTI_ID_ERROR) || (code == FOG_HTTP_PRODUCTI_ID_NOT_SUB) || (code == FOG_HTTP_PRODUCTI_ID_NOT_GATEWAY))
        {
            app_log("subdevice product id is error! code = %ld", code);
            app_log("<===== sub_device_register error <======");
            return err;
        }

        app_log("activate error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== sub_device_register error <======");

        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_rtos_thread_msleep( 200 );
            goto start_subdevice_register;
        }
    }

    return err;
}


//���豸ע��
static OSStatus fog_v2_subdevice_unregister(const char *s_product_id, const char *s_mac)
{
    OSStatus err = kGeneralErr;
    const char *sub_device_unregister_body = "{\"deviceid\":\"%s\"}";
    char http_body[256] = {0};
    int32_t code = -1;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;
    char *subdevice_id = NULL;
    uint32_t index = 0;

    if ( get_sub_device_queue_index_by_mac( &index, s_product_id, s_mac ) == false )
    {
        app_log("mac is error");
        return kGeneralErr;
    }

    if((subdevice_id = get_sub_device_id_by_index(index)) == NULL)
    {
        app_log("subdevice_id is error");
        return kGeneralErr;
    }

start_subdevice_unregister:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdevice_unregister is waitting...");
        mico_thread_msleep(200);
    }

    sprintf(http_body, sub_device_unregister_body, subdevice_id);

    app_log("=====> subdevice unregister send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_UNREGISTER_METHOD, FOG_V2_SUB_UNREGISTER_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    //�������ؽ��
    err = process_response_body(user_http_res.fog_response_body, &code);
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    if(err != kNoErr)
    {
        if(code == FOG_HTTP_DEVICE_ID_ERROR) //�ƶ˿��ܲ��������device id,�����������
        {
            err = kNoErr;
            app_log("[NOTICE]code = 27030!");
            push_cmd_to_subdevice_queue(MQTT_CMD_SUB_UNBIND, subdevice_id);//������Ϣ������
        }
    }

    app_log("subdevice unregister success, device_id:%s", subdevice_id);
    app_log("<===== subdevice unregister success <======");

 exit:
    if(err != kNoErr)
    {
        if ( (code == FOG_HTTP_PRODUCTI_ID_ERROR) || (code == FOG_HTTP_PRODUCTI_ID_NOT_SUB) || (code == FOG_HTTP_PRODUCTI_ID_NOT_GATEWAY))
        {
            app_log("subdevice product id is error! code = %ld", code);
            app_log("<===== subdevice unregister error <======");
            return err;
        }

        app_log("unregister error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== subdevice unregister error <======");

        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_thread_msleep( 200 );
            goto start_subdevice_unregister;
        }
    }

    return err;
}

//���豸����
static OSStatus fog_v2_subdevice_attach(const char *s_product_id, const char *s_mac)
{
    OSStatus err = kGeneralErr;
    const char *sub_device_attach_body = "{\"deviceid\":\"%s\"}";
    char http_body[256] = {0};
    int32_t code = -1;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;
    uint32_t index = 0;
    char *subdevice_id = NULL;

    if(get_sub_device_queue_index_by_mac(&index, s_product_id, s_mac) == false)
    {
        app_log("mac is error");
        return kGeneralErr;
    }

    if((subdevice_id = get_sub_device_id_by_index(index)) == NULL)
    {
        app_log("subdevice_id is error");
        return kGeneralErr;
    }

start_subdevice_attach:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdeice_attach is waitting...");
        mico_thread_msleep(200);
    }

    sprintf(http_body, sub_device_attach_body, subdevice_id);

    app_log("=====> subdevice attach send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_ATTACH_METHOD, FOG_V2_SUB_ATTACH_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    //�������ؽ��
    err = process_response_body(user_http_res.fog_response_body, &code);
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    if(err != kNoErr)
    {
        if(code == FOG_HTTP_DEVICE_ID_ERROR) //�ƶ˿��ܲ��������device id,�����������,���ӳ����쳣
        {
            app_log("[ATTACH ERROR]code = 27030!");
            app_log("<===== subdevice attach error <======");

            push_cmd_to_subdevice_queue(MQTT_CMD_SUB_UNBIND, subdevice_id);//������Ϣ������

            return kGeneralErr;
        }else if ( (code == FOG_HTTP_PRODUCTI_ID_ERROR) || (code == FOG_HTTP_PRODUCTI_ID_NOT_SUB) || (code == FOG_HTTP_PRODUCTI_ID_NOT_GATEWAY))
        {
            app_log("subdevice product id is error! code = %ld", code);
            app_log("<===== subdevice attach error <======");
            return err;
        }
    }

    app_log("subdevice attach success, sub device_id:%s", subdevice_id);
    app_log("<===== subdevice attach success <======");

 exit:
    if(err != kNoErr)
    {
        app_log("subdeice attach error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== subdevice attach error <======");
        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_thread_msleep( 200 );
            goto start_subdevice_attach;
        }
    }

    return err;
}


//���豸�Ͽ�����
static OSStatus fog_v2_subdevice_detach(const char *s_product_id, const char *s_mac)
{
    OSStatus err = kGeneralErr;
    const char *sub_device_detach_body = "{\"deviceid\":\"%s\"}";
    char http_body[256] = {0};
    int32_t code = -1;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;
    char *subdevice_id = NULL;
    uint32_t index = 0;

    if(get_sub_device_queue_index_by_mac(&index, s_product_id, s_mac) == false)
    {
        app_log("mac is error");
        return kGeneralErr;
    }

    if((subdevice_id = get_sub_device_id_by_index(index)) == NULL)
    {
        app_log("subdevice_id is error");
        return kGeneralErr;
    }

start_subdeice_detach:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdevice_detach is waitting...");
        mico_thread_msleep(200);
    }

    sprintf(http_body, sub_device_detach_body, subdevice_id);

    app_log("=====> subdevice detach send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_DETACH_METHOD, FOG_V2_SUB_DETACH_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    //�������ؽ��
    err = process_response_body(user_http_res.fog_response_body, &code);
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    {
        if(code == FOG_HTTP_DEVICE_ID_ERROR) //�ƶ˿��ܲ��������device id,�����������,���ӳ����쳣
        {
            app_log("[DETACH ERROR]code = 27030!");
            app_log("<===== subdevice detach error <======");

            push_cmd_to_subdevice_queue(MQTT_CMD_SUB_UNBIND, subdevice_id);//������Ϣ������

            return kGeneralErr;
        }else if ( (code == FOG_HTTP_PRODUCTI_ID_ERROR) || (code == FOG_HTTP_PRODUCTI_ID_NOT_SUB) || (code == FOG_HTTP_PRODUCTI_ID_NOT_GATEWAY))
        {
            app_log("subdevice product id is error! code = %ld", code);
            app_log("<===== subdevice detach error <======");
            return err;
        }
    }

    app_log("subdevice detach success, sub device_id:%s", subdevice_id);
    app_log("<===== subdevice detach success <======");

 exit:
    if(err != kNoErr)
    {
        app_log("subdeice deaach error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== subdevice detach error <======");

        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_thread_msleep( 200 );
            goto start_subdeice_detach;
        }
    }

    return err;
}

//���豸���ӳ�ʱ
OSStatus fog_v2_subdevice_add_timeout(const char *s_product_id)
{
    OSStatus err = kGeneralErr;
    const char *sub_device_add_timeout_body = "{\"productid\":\"%s\"}";
    char http_body[256] = {0};
    int32_t code = -1;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;

start_add_timeout:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdevice_add_timeout is waitting...");
        mico_thread_msleep(200);
    }

    sprintf(http_body, sub_device_add_timeout_body, s_product_id);

    app_log("=====> subdevice add timeout send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_ADD_TIMEOUT_METHOD, FOG_V2_SUB_ADD_TIMEOUT_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    //�������ؽ��
    err = process_response_body(user_http_res.fog_response_body, &code);
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    require_noerr( err, exit );

    app_log("subdevice send add timeout success, product id:%s", s_product_id);
    app_log("<===== subdevice add timeout success <======");

 exit:
    if(err != kNoErr)
    {
        app_log("subdeice add timeout error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== subdevice add timeout error <======");
        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_thread_msleep( 200 );
            goto start_add_timeout;
        }
    }

    return err;
}

//��ȡ���豸�б�
OSStatus fog_v2_subdeice_get_list(char *http_response, uint32_t recv_len, bool *get_http_response_success)
{
    OSStatus err = kGeneralErr;

    char http_body[256] = {0};
    int32_t code = -1;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;

start_get_list:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdeice_get_list is waitting...");
        mico_thread_msleep(200);
    }

    app_log("=====> subdevice get list send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_GET_LIST_METHOD, FOG_V2_SUB_GET_LIST_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    //�������ؽ��
    err = process_response_body(user_http_res.fog_response_body, &code);
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    require_noerr( err, exit );

    app_log("body:%s", user_http_res.fog_response_body);

    if(recv_len > strlen(user_http_res.fog_response_body))
    {
        memcpy(http_response, user_http_res.fog_response_body, strlen(user_http_res.fog_response_body));
        *get_http_response_success = true;
        app_log("[SUCCESS] copy subdevice list response!");
    }else
    {
        *get_http_response_success = false;
        app_log("[ERROR] recv_len size is small!");
    }

    app_log("<===== subdevice get list success <======");

 exit:
    if(err != kNoErr)
    {
        app_log("subdeice get list error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== subdevice get list error <======");

        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_thread_msleep( 200 );
            goto start_get_list;
        }
    }

    return err;
}


//���豸��������
static OSStatus fog_v2_subdevice_send_event(const char *payload, const char *s_product_id, const char *s_mac, uint32_t type)
{
    OSStatus err = kGeneralErr;
    int32_t code = -1;
    json_object *send_json_object = NULL;
    char *http_body = NULL;
    FOG_HTTP_RESPONSE_SETTING_S user_http_res;
    char *subdevice_id = NULL;
    uint32_t index = 0;

    if(fog_v2_is_have_superuser() == false)
    {
        app_log("[ERROR]gateway don't have superuser!");
        return kGeneralErr;
    }

start_sub_send_event:
    while(get_https_connect_status() == false)
    {
        app_log("https disconnect, fog_v2_subdevice_send_event is waitting...");
        mico_thread_msleep(200);
    }

    if(get_sub_device_queue_index_by_mac(&index, s_product_id, s_mac) == false)
    {
        app_log("mac is error");
        return kGeneralErr;
    }

    if((subdevice_id = get_sub_device_id_by_index(index)) == NULL)
    {
        app_log("subdevice_id is error");
        return kGeneralErr;
    }

    send_json_object = json_object_new_object();
    require_string(send_json_object != NULL, exit, "json_object_new_object() error");

    json_object_object_add(send_json_object, "subdeviceid", json_object_new_string(subdevice_id));
    json_object_object_add(send_json_object, "flag", json_object_new_int(type));
    json_object_object_add(send_json_object, "format", json_object_new_string("json"));
    json_object_object_add(send_json_object, "payload", json_object_new_string(payload));
    http_body = (char *)json_object_to_json_string(send_json_object);
    require_action_string(http_body != NULL, exit, err = kGeneralErr, "json_object_to_json_string() is error");

    app_log("=====> subdevice send_event send ======>");

    err = fog_v2_push_http_req_mutex(true, FOG_V2_SUB_SENDEVENT_METHOD, FOG_V2_SUB_SENDEVENT_URI, FOG_V2_HTTP_DOMAIN_NAME, http_body, &user_http_res);
    require_noerr( err, exit );

    user_free_json_obj(&send_json_object);

    //�������ؽ��
    err = process_response_body(user_http_res.fog_response_body, &code);
    require_noerr( err, exit );

    err = check_http_body_code(code);   //�����token�����Ǵ������⣬�����ڲ��ᴦ�����֮���ٷ���
    require_noerr( err, exit );

    app_log("<===== subdevice send_event success <======");

 exit:
     user_free_json_obj(&send_json_object);

    if(err != kNoErr)
    {
        app_log("subdeice send_event error, send_status:%d, status_code:%ld", user_http_res.send_status, user_http_res.status_code);
        app_log("<===== subdevice send_event error <======");
        if ( (HTTP_CONNECT_ERROR == user_http_res.send_status) || (HTTP_RESPONSE_FAILURE == user_http_res.send_status))
        {
            mico_thread_msleep( 200 );
            goto start_sub_send_event;
        }
    }

    return err;
}


OSStatus add_mqtt_topic_by_mac(const char *s_product_id, const char *mac)
{
    OSStatus err = kGeneralErr;

    if(mac == NULL || s_product_id == NULL)
    {
        app_log("param error!");
        return kGeneralErr;
    }

    err = add_mqtt_topic_command_by_mac(s_product_id, mac);
    require_noerr(err, exit);

//    err = add_mqtt_topic_cmd_by_mac(s_product_id, mac);
//    require_noerr(err, exit);

    exit:
    return err;
}

static OSStatus add_mqtt_topic_command_by_mac(const char *s_product_id, const char *s_mac)
{
    uint32_t index = 0;
    char *commands_topic = NULL;
    OSStatus err = kGeneralErr;

    if(s_mac == NULL || s_product_id == NULL)
    {
        app_log("[ERROR]param error");
        return kGeneralErr;
    }

    if(get_sub_device_queue_index_by_mac(&index, s_product_id, s_mac) == false)
    {
        app_log("[ERROR]mac is error");
        return kGeneralErr;
    }

    if((commands_topic = get_sub_device_commands_topic_by_index(index)) == NULL)
    {
        app_log("[ERROR]commands_topic is NULL");
        return kGeneralErr;
    }

    err = add_mqtt_topic_command( commands_topic );
    if ( err != kNoErr )
    {
        app_log("command err = %d", err);
    }

    return err;
}

//static OSStatus add_mqtt_topic_cmd_by_mac(const char *s_product_id, const char *s_mac )
//{
//    uint32_t index = 0;
//    char *cmd_topic = NULL;
//    OSStatus err = kGeneralErr;
//
//    if ( s_mac == NULL )
//    {
//        app_log("[ERROR]s_mac is NULL");
//        return kGeneralErr;
//    }
//
//    if ( get_sub_device_queue_index_by_mac( &index, s_product_id, s_mac ) == false )
//    {
//        app_log("[ERROR]mac is error");
//        return kGeneralErr;
//    }
//
//    if ( (cmd_topic = get_sub_device_cmd_topic_by_index( index )) == NULL )
//    {
//        app_log("[ERROR]cmd_topic is NULL");
//        return kGeneralErr;
//    }
//
//    err = add_mqtt_topic_cmd( cmd_topic );
//    if ( err != kNoErr )
//    {
//        app_log("command err = %d", err);
//    }
//
//    return err;
//}

OSStatus remove_mqtt_topic_by_mac(const char *s_product_id, const char *s_mac)
{
    uint32_t index = 0;
//    char *cmd_topic = NULL;
    char *commands_topic = NULL;
    OSStatus err = kGeneralErr;

    if ( s_mac == NULL )
    {
        app_log("[ERROR]s_mac is NULL");
        return kGeneralErr;
    }

    if ( get_sub_device_queue_index_by_mac( &index, s_product_id, s_mac ) == false )
    {
        app_log("[ERROR]mac is error");
        return kGeneralErr;
    }

    if ( (commands_topic = get_sub_device_commands_topic_by_index( index )) == NULL )
    {
        app_log("[ERROR]commands_topic is NULL");
        return kGeneralErr;
    }

    err = remove_mqtt_topic( commands_topic );
    if ( err != kNoErr )
    {
        app_log("remove command err = %d, mac = %s", err, s_mac);
    }

//    if ( (cmd_topic = get_sub_device_cmd_topic_by_index( index )) == NULL )
//    {
//        app_log("[ERROR]cmd_topic is NULL");
//        return kGeneralErr;
//    }

//    err = remove_mqtt_topic( cmd_topic );
//    if ( err != kNoErr )
//    {
//        app_log("remove cmd err = %d, mac = %s", err, s_mac);
//    }

    return err;
}

//���ܣ�����һ�����豸
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_add_subdevice( const char *s_product_id, const char *s_mac, bool set_auto_online)
{
    OSStatus err = kGeneralErr;

    require_action((s_product_id != NULL && s_mac != NULL), exit, err = kGeneralErr);

    //���豸ע��
    err = fog_v2_subdevice_register( s_product_id, s_mac );//�ڲ����������豸��Դ
    require_noerr( err, exit );

    //Ƶ������
    err = add_mqtt_topic_by_mac( s_product_id, s_mac );
    require_noerr( err, exit );

    if(set_auto_online == true)
    {
        //�������豸����
        err = fog_v2_set_subdevice_status(s_product_id, s_mac, true);
        require_noerr( err, exit );
    }

    app_log("register, add mqtt topic, set online! product_id:%s, mac:%s", s_product_id, s_mac);

    exit:
    if(err != kNoErr)
    {
        fog_v2_remove_subdevice(s_product_id, s_mac);
        app_log("fog_v2_add_subdevice() failed, remove it now~");
    }

    return err;
}

//���ܣ�ɾ��һ�����豸
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_remove_subdevice( const char *s_product_id, const char *s_mac )
{
    OSStatus err = kGeneralErr;

    require_action((s_product_id != NULL && s_mac != NULL), exit, err = kGeneralErr);

    //�������豸����
    err = fog_v2_set_subdevice_status( s_product_id, s_mac, false );
    if(err != kNoErr)
    {
        app_log("set offline error, product id:%s, mac:%s", s_product_id, s_mac);
    }

    //���豸ȡ������topic
    err = remove_mqtt_topic_by_mac( s_product_id, s_mac );
    if(err != kNoErr)
    {
        app_log("remove topic error, product id:%s, mac:%s", s_product_id, s_mac);
    }

    //���豸ע��
    err = fog_v2_subdevice_unregister( s_product_id, s_mac ); //�ڲ����ͷ����豸��Դ
    if(err != kNoErr)
    {
        app_log("unregister error, product id:%s, mac:%s", s_product_id, s_mac);
    }

    if(sub_device_queue_put_by_mac(s_product_id, s_mac) == false) //�ͷ���Դ
    {
        app_log("sub_device_queue_put_by_mac() error!!! product id:%s, mac:%s", s_product_id, s_mac);
    }

    app_log("unregister! remove mqtt topic! set offline! release subdevice!");

    exit:
    return err;
}

//���ܣ��������豸��������״̬
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//������online - ���豸�Ƿ�����
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_set_subdevice_status(const char *s_product_id, const char *s_mac, bool online)
{
    OSStatus err = kGeneralErr;

    require_action((s_product_id != NULL && s_mac != NULL), exit, err = kGeneralErr);

    if ( online == true )
    {
        //���豸����
        err = fog_v2_subdevice_attach( s_product_id, s_mac );
        require_noerr( err, exit );
    } else
    {
        //���豸�Ͽ����� detach
        err = fog_v2_subdevice_detach( s_product_id, s_mac );
        require_noerr( err, exit );
    }

    exit:
    return err;
}


//���ܣ����豸��������
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//������ flag - ���ͷ�ʽ
//���������궨�����,���������ϵķ�ʽ
//FOG_V2_SEND_EVENT_RULES_PUBLISH  ���豸��topicȥpublish����
//FOG_V2_SEND_EVENT_RULES_DATEBASE ���˴ε�payload���ݴ������ݿ�
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_subdevice_send(const char *s_product_id, const char *s_mac, const char *payload, uint32_t flag)
{
    OSStatus err = kGeneralErr;

    require_action((s_product_id != NULL && s_mac != NULL && payload != NULL), exit, err = kGeneralErr);

    err = fog_v2_subdevice_send_event(payload, s_product_id, s_mac, flag);
    require_noerr( err, exit );

    exit:
    return err;
}

//���ܣ����豸��������
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//������ payload - �������ݻ�������ַ
//������ payload_len - �������ݻ�������ַ�ĳ���
//������ timeout - �������ݵĳ�ʱʱ��
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_subdevice_recv(const char *s_product_id, const char *s_mac, char *payload, uint32_t payload_len, uint32_t timeout)
{
    OSStatus err = kGeneralErr;
    uint32_t index = 0;
    SUBDEVICE_RECV_DATA_S *subdevice_recv_p = NULL;
    mico_queue_t *sub_device_queue_p = NULL;

    require_action( (s_mac != NULL && s_product_id != NULL && payload != NULL && payload_len != 0), exit, err = kGeneralErr );

    if(get_sub_device_queue_index_by_mac(&index, s_product_id, s_mac) == false)
    {
        app_log("get_sub_device_queue_index_by_mac error");
        return kGeneralErr;
    }

    sub_device_queue_p = get_sub_device_queue_addr_by_index(index);
    require(sub_device_queue_p != NULL, exit);

    err = mico_rtos_pop_from_queue(sub_device_queue_p, &subdevice_recv_p, timeout);
    require_noerr_string( err, exit, "queue is full!!!!");
    require_noerr_action_string(err, exit, app_log("product id:%s, mac:%s queue is full!", s_product_id, s_mac); ,"mico_rtos_pop_from_queue() error");

    require_action_string( subdevice_recv_p != NULL, exit, err = kGeneralErr, "subdevice_recv_p is NULL");

    require_action_string( payload_len > subdevice_recv_p->data_len, exit, err = kGeneralErr, "payload_len is too short");

    memset(payload, 0, payload_len);
    memcpy(payload, subdevice_recv_p->data, subdevice_recv_p->data_len);

    err = kNoErr;
  exit:
    if ( subdevice_recv_p != NULL )
    {
        if ( subdevice_recv_p->data != NULL )
        {
            free( subdevice_recv_p->data );
            subdevice_recv_p->data = NULL;
        }
        free( subdevice_recv_p ); //�ͷű���
        subdevice_recv_p = NULL;
    }

    return err;
}




//-------------------------���豸�����߳��������------------------

//�����Ƿ��Ѿ���ʼ��
bool is_subdevice_cmd_queue_init(void)
{
    if(sub_device_cmd_queue == NULL)
        return false;
    else
        return true;
}


//�������в���һ������
OSStatus push_cmd_to_subdevice_queue(SUBDEVICE_CMD_TYPE type, const char *device_id)
{
    SUBDEVICE_RECV_CMD_DATA_S cmd_msg;
    OSStatus err = kGeneralErr;

    if(is_subdevice_cmd_queue_init() == false)
    {
        app_log("is_subdevice_cmd_queue_init() return false!");
        return kGeneralErr;
    }

    if(MQTT_CMD_GATEWAY_UNBIND != type && MQTT_CMD_GATEWAY_BIND != type && MQTT_CMD_SUB_UNBIND != type)
    {
        app_log("type error");
        return false;
    }

    if(device_id == NULL)
    {
        app_log("device_id is NULL");
        return false;
    }

    if ( strlen( device_id ) >= sizeof(cmd_msg.device_id) )
    {
        app_log("device_id is too long");
        return false;
    }

    cmd_msg.cmd_type = type;
    strcpy(cmd_msg.device_id, device_id);

    err = mico_rtos_push_to_queue( &sub_device_cmd_queue, &cmd_msg, 100 );
    if ( kNoErr != err )
    {
        app_log("[error]push msg into sub_device_cmd_queue, err=%d", err);
    } else
    {
        app_log("push cmd to queue success!");
    }

    return err;
}


//��غ���
void gateway_bind_monitor(mico_thread_arg_t arg)
{
    OSStatus err = kGeneralErr;
    SUBDEVICE_RECV_CMD_DATA_S cmd_msg;
    uint32_t index = 0;
    char s_product_id[64] = {0};
    char s_mac[16] = {0};

    app_log("----------------gateway_bind_monitor thread start----------------");

    err = mico_rtos_init_queue( &sub_device_cmd_queue, "sub device cmd queue", sizeof(SUBDEVICE_RECV_CMD_DATA_S), 2 );
    require_noerr( err, exit );

    while(1)
    {
        memset(&cmd_msg, 0, sizeof(cmd_msg));
        mico_rtos_pop_from_queue(&sub_device_cmd_queue, &cmd_msg, MICO_NEVER_TIMEOUT);

        if(cmd_msg.cmd_type == MQTT_CMD_GATEWAY_UNBIND)
        {
            app_log("cmd type:gateway unbind! deviceid:%s", cmd_msg.device_id);

            user_fog_v2_device_notification(MQTT_CMD_GATEWAY_UNBIND, NULL, NULL);
        }else if(cmd_msg.cmd_type == MQTT_CMD_GATEWAY_BIND)
        {
            app_log("cmd type:gateway bind! deviceid:%s", cmd_msg.device_id);

            user_fog_v2_device_notification(MQTT_CMD_GATEWAY_BIND, NULL, NULL);
        }else if(cmd_msg.cmd_type == MQTT_CMD_SUB_UNBIND)
        {
            app_log("cmd type:subdevice unbind! deviceid:%s", cmd_msg.device_id);

            if(get_sub_device_queue_index_by_deviceid(&index ,cmd_msg.device_id) == true)
            {
                if(get_sub_device_product_id_by_index(index) != NULL && get_sub_device_mac_by_index(index) != NULL)
                {
                    memset(s_product_id, 0, sizeof(s_product_id));
                    memset(s_mac, 0, sizeof(s_mac));

                    strcpy(s_product_id ,(const char *)get_sub_device_product_id_by_index(index));
                    strcpy(s_mac ,(const char *)get_sub_device_mac_by_index(index));

                    fog_v2_remove_subdevice((const char *)get_sub_device_product_id_by_index(index), (const char *)get_sub_device_mac_by_index(index));

                    user_fog_v2_device_notification(MQTT_CMD_SUB_UNBIND, s_product_id, s_mac);
                }
            }else
            {
                app_log("deviceid is not available, %s", cmd_msg.device_id);
            }
        }else
        {
            app_log("cmd type error!");
        }
    }

 exit:
    mico_rtos_delete_thread(NULL);
}


void start_gateway_bind_monitor(void)
{
    OSStatus err = kGeneralErr;

    /* Create a new thread */
    err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "gateway bind monitor", gateway_bind_monitor, 0x1000, (uint32_t) NULL );
    require_noerr_string( err, exit, "ERROR: Unable to start the send_event_test thread" );

 exit:
    return;
}

//-------------------------���豸�����߳��������------------------

#endif


