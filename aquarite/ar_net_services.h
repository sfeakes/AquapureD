#ifndef AR_NET_SERVICES_H_
#define AR_NET_SERVICES_H_

//void main_server();
//void main_server_TEST(struct aqualinkdata *aqdata, char *s_http_port);
//bool start_web_server(struct mg_mgr *mgr, struct aqualinkdata *aqdata, char *port, char* web_root);
//bool start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata, struct aqconfig *aqconfig);
bool start_net_services(struct mg_mgr *mgr, struct aqconfig *config, struct arconfig *ar_prms);
bool broadcast_aquaritestate(struct mg_connection *nc);
void check_net_services(struct mg_mgr *mgr);
//void broadcast_aqualinkstate(struct mg_connection *nc);
//void broadcast_aqualinkstate_error(struct mg_connection *nc, char *msg);


#endif // WEB_SERVER_H_