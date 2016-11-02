#ifndef _FOG_V2_FOG_V2_INCLUDE_H_
#define _FOG_V2_FOG_V2_INCLUDE_H_

#include "mico.h"
#include "fog_v2_config.h"

#include "./fog_bonjour/fog_bonjour.h"

#include "./fog_http/fog_http_client.h"
#include "./fog_http/fog_v2_sub.h"
#include "./fog_http/fog_v2_sub_device_management.h"
#include "./fog_http/fog_v2.h"

#include "./fog_mqtt/fog_mqtt.h"
#include "./fog_mqtt/fog_process_mqtt_cmd.h"

#include "./fog_ota/fog_ota.h"

#include "./fog_tcp_server/fog_tcp_server.h"


//-------�û����ýӿ�-------
//���ܣ���ʼ��fog����
//��������
//����ֵ��kNoErrΪ�ɹ� ����Ϊʧ��
OSStatus init_fog_v2_service(void);

//���ܣ�����fog�ķ���
//��������
//����ֵ��kNoErrΪ�ɹ� ����Ϊʧ��
OSStatus start_fog_v2_service(void);

//���ܣ����ƶ˷�������
//������ payload - Ҫ���ƶ˷��͵����ݣ���ָ��ָ�������ֻ��Ϊjson��ʽ
//������ flag - ���ͷ�ʽ
//���������궨�����,���������ϵķ�ʽ
//FOG_V2_SEND_EVENT_RULES_PUBLISH  ���豸��topicȥpublish����
//FOG_V2_SEND_EVENT_RULES_DATEBASE ���˴ε�payload���ݴ������ݿ�
//FOG_V2_SEND_EVENT_RULES_PRODUCT  ���豸��Ӧ��Ʒ��topicȥpublish����(�������͸�����)
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_device_send_event(const char *payload, uint32_t flag);

//���ܣ����ƶ˽�������
//������ payload - �������ݻ�������ַ
//������ payload_len - �������ݻ�������ַ�ĳ���
//������ timeout - �������ݵȴ�ʱ��
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_device_recv_command(char *payload, uint32_t payload_len, uint32_t timeout);

//���ܣ��豸�����û��ձ�־λ �����øýӿڣ��豸�������������Զ����ƶ˷����豸��������
//��������
//����ֵ����
void fog_v2_set_device_recovery_flag(void);

//���ܣ���ѯ���豸�Ƿ��г����û�������)
//��������
//����ֵ��true - �г����û���flase - �޳����û�
bool fog_v2_is_have_superuser(void);


#if (FOG_V2_USE_SUB_DEVICE == 1)
//-------���豸��ؽӿ�-------
//ע�⣺Ҫ�����������豸API,����Ҫ��Ӧ�ó���config/fog_v2_config.h�� FOG_V2_USE_SUB_DEVICE �궨�����Ϊ
//#define FOG_V2_USE_SUB_DEVICE  (1)������Ч��

//���ܣ����һ�����豸
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_add_subdevice( const char *s_product_id, const char *s_mac, bool auto_set_online);

//���ܣ�ɾ��һ�����豸
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_remove_subdevice( const char *s_product_id, const char *s_mac );

//���ܣ��������豸��������״̬
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//������online - ���豸�Ƿ�����
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_set_subdevice_status(const char *s_product_id, const char *s_mac, bool online);

//���ܣ����豸��������
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//������ flag - ���ͷ�ʽ
//���������궨�����,���������ϵķ�ʽ
//FOG_V2_SEND_EVENT_RULES_PUBLISH  ���豸��topicȥpublish����
//FOG_V2_SEND_EVENT_RULES_DATEBASE ���˴ε�payload���ݴ������ݿ�
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_subdevice_send(const char *s_product_id, const char *s_mac, const char *payload, uint32_t flag);

//���ܣ����豸��������
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//������ payload - �������ݻ�������ַ
//������ payload_len - �������ݻ�������ַ�ĳ���
//������ timeout - �������ݵĳ�ʱʱ��
//����ֵ��kNoErrΪ�ɹ� ����ֵΪʧ��
OSStatus fog_v2_subdevice_recv(const char *s_product_id, const char *s_mac, char *payload, uint32_t payload_len, uint32_t timeout);

//���ܣ��豸״̬�ı�֪ͨ���ú�����Ҫ�û����Լ��Ĵ����и�д�����壬�м����ʹ��WEAK������øú�����
//������type - �¼�����
//�¼�����һ��������
//MQTT_CMD_GATEWAY_UNBIND   �����豸�����(s_product_id��s_mac��Ч)
//MQTT_CMD_GATEWAY_BIND     �����豸����(s_product_id��s_mac��Ч)
//MQTT_CMD_SUB_UNBIND       ���豸�����(s_product_id��s_mac��Ч)
//������ s_product_id - ���豸��ƷID
//������ s_mac - ���豸MAC��ַ
//����ֵ����
//USED void user_fog_v2_device_notification(SUBDEVICE_CMD_TYPE type, const char *s_product_id, const char *s_mac)
//{
//    //user todo....
//}

#endif //end of FOG_V2_USE_SUB_DEVICE

#endif
