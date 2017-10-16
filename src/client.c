/**
 *
 *    GFW.Press
 *    Copyright (C) 2016  chinashiyu ( chinashiyu@gfw.press ; http://gfw.press )
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 **/
 
/*
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
*/




#include <winsock2.h>
#include <ws2tcpip.h>






#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/md5.h>






#define MSG_NOSIGNAL 0



#include <errno.h>
#include <signal.h>
#include <time.h> 



extern int get_password_key(char * password, char * key);

extern int decrypt(char *key, char *in, int inl, char *out);

extern int encrypt_net(char *key, char *in, int in_len, char *out);

/** ���ݿ�(������)���ֵ��768K */
static int BUFFER_MAX = 1024 * 768;

/**  ���������ֵ��512K */
static int BUFFER_SIZE_MAX = 1024 * 512;

/**  �������Զ������Ĳ���ֵ��128K */
/** static int BUFFER_SIZE_STEP = 1024 * 128; */

/**  �������ݳ���ֵ���ܺ���ֽڳ��ȣ��̶�30���ֽڣ����ܺ�̶�14���ֽ� */
static int ENCRYPT_SIZE = 30;

/**  ����������󳤶ȣ�4K */
static int NOISE_MAX = 1024 * 4;

/** ���ݿ��С���ַ������� 14 */
static int SIZE_SIZE = 14;

/**  IV�ֽڳ��ȣ�16 */
static int IV_SIZE = 16;

static char * server_host;

static int server_port;

static int listen_port;

static char * password;

static char * key;

/** IO�̲߳����ṹ */
struct IO {

	int socket_agent;

	int socket_server;

};

int random_load = 0;
int get_random_load(){
	return 1024+(rand() % 65536);
}
int recv_len_seed = 30;

/**
 * ��ȡ��ǰʱ��ʱ��
 */
void datetime(char *_datetime) {

	time_t _time = time(NULL);

	struct tm *_tm = localtime(&_time);

	sprintf(_datetime, "%02d-%02d-%02d %02d:%02d:%02d",
		_tm->tm_year + 1900, _tm->tm_mon + 1, _tm->tm_mday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);

}

/**
 * ��ӡ��Ϣ
 */
void _log(char * message) {

	char * _datetime = malloc(22);

	datetime(_datetime);

	_datetime[21] = '\0';

	printf("\n[%s] %s\n", _datetime, message);

	free(_datetime);

}

/**
 * �������ݿ����ݳ��Ⱥ���������
 */
int get_block_sizes(char *head, int *sizes) {

	if (strlen(head) != 14) {

		return -1;

	}

	int data, noise;

	sscanf(head, "%08d,%05d", &data, &noise);

	sizes[0] = data;

	sizes[1] = noise;

	if (sizes[0] < 1 || sizes[0] > BUFFER_SIZE_MAX || sizes[1] < 0) {

		return -1;

	}

	if (sizes[0] + sizes[1] > BUFFER_MAX) {

		return -1;

	}

	return 2;

}


int get_buffer_size_min(){
	return 16 + (rand() % random_load);
}

int get_head_block(){
	return recv_len_seed + (rand() % random_load);
}


/**
 * �����IO�߳�
 */
void *thread_io_agent(void *_io) {

	struct IO io = *((struct IO*) _io);

	while (1) {
		int buffer_size_min = get_buffer_size_min();
		char *buffer = malloc(buffer_size_min + 1);

		/** ������������� */
		int recvl = recv(io.socket_agent, buffer, buffer_size_min, MSG_NOSIGNAL);

		if (recvl < 1) {

			free(buffer);

			break;

		}

		buffer[recvl] = '\0';

		int outl = recvl + ENCRYPT_SIZE + NOISE_MAX;

		char *out = malloc(outl + 1);

		int _outl = encrypt_net(key, buffer, recvl, out);

		out[_outl] = '\0';

		free(buffer);

		/** �������ݵ������� */
		int sendl = send(io.socket_server, out, _outl, MSG_NOSIGNAL);

		free(out);

		if (sendl < 1) {

			break;

		}

	}

	pthread_exit(0);
return 0;
}

/**
 * ������IO�߳�
 */
void *thread_io_server(void *_io) {

	struct IO io = *((struct IO*) _io);
	int free_head_temp = 1;
	int head_temp_last = 0;
	int head_len_temp = 0;
	char *head_temp;

	while (1) {
		if(free_head_temp){
			int head_block_length = get_head_block();
			head_temp = malloc(head_block_length);
			/** ���շ�����ͷ���� */
			head_len_temp = recv(io.socket_server, head_temp, head_block_length, MSG_NOSIGNAL);

			if (head_len_temp < ENCRYPT_SIZE) {
				free(head_temp);
				break;
			}
		}else{
			int head_block_length = get_head_block();
			char *head_temp_temp = malloc(head_block_length);
			memcpy(head_temp_temp, &head_temp[head_len_temp-head_temp_last], head_temp_last);
			free(head_temp);
			head_temp = head_temp_temp;
			/** ���շ�����ͷ���� */
			head_len_temp = recv(io.socket_server, &head_temp[head_temp_last], head_block_length-head_temp_last, MSG_NOSIGNAL);

			head_len_temp = head_len_temp + head_temp_last;
			if (head_len_temp < ENCRYPT_SIZE) {
				free(head_temp);
				break;
			}
		}

		int head_len = ENCRYPT_SIZE;
		
		int head_else = head_len_temp - head_len;

		char *head = malloc(ENCRYPT_SIZE + 1);

		memcpy(head, head_temp, head_len);

		head[ENCRYPT_SIZE] = '\0';

		char *head_out = malloc(SIZE_SIZE + 1);

		if (decrypt(key, head, head_len, head_out) == -1) {

			free(head_out);

			break;

		}

		head_out[SIZE_SIZE] = '\0';

		free(head);

		int *sizes = malloc(2 * sizeof(int));

		if (get_block_sizes(head_out, sizes) == -1) {

			free(head_out);

			free(sizes);

			break;

		}

		int data = (int) sizes[0];

		int noise = (int) sizes[1];

		free(head_out);

		free(sizes);

		int size = data + noise;

		char *in = malloc(size + 1);

		int _size = 0;
		if(head_else > size){
			memcpy(in, &head_temp[head_len], size);
			head_temp_last = head_else-size;
			if(head_temp_last<ENCRYPT_SIZE){
				recv_len_seed = ENCRYPT_SIZE;
			}else{
				recv_len_seed = head_temp_last;
			}
			free_head_temp = 0;
			_size = size;
		}else{
			if(head_else>0){
				memcpy(in, &head_temp[head_len], head_else);
			}
			free(head_temp);
			head_temp_last = 0;
			free_head_temp = 1;
			_size = head_else;
		}

		for (; _size < size;) {

			/** ���շ��������� */
			head_len = recv(io.socket_server, &in[_size], (size - _size), MSG_NOSIGNAL);

			if (head_len < 1) {

				break;

			}

			_size += head_len;

		}

		if (_size != size) {

			free(in);

			break;

		}

		in[size] = '\0';

		int outl = data - IV_SIZE;

		char * out = malloc(outl + 1);

		if (decrypt(key, in, data, out) == -1) {

			free(in);

			free(out);

			break;

		}

		out[outl] = '\0';

		free(in);

		/** ת�����ݵ������ */
		int sendl = send(io.socket_agent, out, outl, MSG_NOSIGNAL);

		free(out);

		if (sendl < 1) {
			break;
		}

	}
	if(!free_head_temp){
		free(head_temp);
	}
	pthread_exit(0);
return 0;
}

/**
 * ����socket��ʱ, 180��
 */
void set_timeout(int socket) {
	int timeout = 180000;
	/*
	struct timeval tv;
	tv.tv_sec = 180;
	tv.tv_usec = 0;
	
	//(struct timeval *) &tv,
	//sizeof(struct timeval)
 */
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *) &timeout, sizeof(timeout));

	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(timeout));

}

/**
 * ���ӷ�����
 */
int connect_server() {

	WSADATA wsaData={0};
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
        	_log("Error at WSAStartup()\n");

	int socket_server = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_server == INVALID_SOCKET)
	{
		/*printf("Error at socket(): %ld\n", WSAGetLastError());//�Լ���Ϲ���*/
		_log("���ӷ�����ʧ���ˣ��޷������������");
		return -1;
	}
	struct sockaddr_in sockaddr_server;

	memset(&sockaddr_server, 0, sizeof(sockaddr_server));

	sockaddr_server.sin_family = AF_INET;

	/** htonl(INADDR_LOOPBACK); */
	sockaddr_server.sin_addr.s_addr = inet_addr(server_host);

	sockaddr_server.sin_port = htons(server_port);

	if (connect(socket_server, (struct sockaddr *) &sockaddr_server, sizeof(struct sockaddr)) == -1) {

		char message[128];

		sprintf(message, "���ӷ�����ʧ�ܣ�%s:%d", server_host, server_port);

		_log(message);

		return -1;

	}

	return socket_server;

}

/**
 * �ͻ����߳�
 */
void *thread_client(void *_socket_agent) {

	pthread_detach(pthread_self());

	struct IO io;

	io.socket_agent = *((int *) _socket_agent);

	io.socket_server = connect_server();

	if (io.socket_server == -1) {

		close(io.socket_server);

		pthread_exit(0);

	}

	set_timeout(io.socket_agent);

	set_timeout(io.socket_server);

	pthread_t thread_id_agent;

	pthread_create(&thread_id_agent, NULL, thread_io_agent, (void *) &io);

	pthread_t thread_id_server;

	pthread_create(&thread_id_server, NULL, thread_io_server, (void *) &io);

	pthread_join(thread_id_agent, NULL);

	pthread_join(thread_id_server, NULL);

	close(io.socket_agent);

	close(io.socket_server);

	pthread_exit(0);
return 0;
}

int set_config(char *_server_host, char *_server_port, char *_password, char *_listen_port) {

	if (strlen(_server_host) < 7 || strlen(_server_port) < 2 || strlen(_listen_port) < 2 || strlen(_password) < 8) {

		return -1;

	}

	server_host = _server_host;

	server_port = atoi(_server_port);

	listen_port = atoi(_listen_port);

	password = _password;

	key = malloc(25);

	get_password_key(password, key);

	key[24] = '\0';

	return 0;

}

/**
 * ��ʼ������
 */
int load_config() {

	char *_server_host = malloc(128);

	char *_server_port = malloc(8);

	char *_password = malloc(32);

	char *_listen_port = malloc(8);

	char * text = malloc(2048);

	FILE* file;

	if ((file = fopen("client.json", "r+")) == NULL) {

		return -1;

	}

	int pos = 0;

	while (!feof(file)) {

		fscanf(file, "%s", &text[pos]);

		pos = strlen(text);

	}

	if (pos == 0) {

		return -1;

	}

	fclose(file);

	char _text[pos + 1];

	int _pos = 0;

	int i;

	for (i = 0; i < pos; i++) {

		char c = text[i];

		if (c != '{' && c != '}' && c != '"') {

			_text[_pos++] = c;

		}

	}

	free(text);

	if (_pos == 0) {

		return -1;

	}

	_text[_pos] = '\0';

	sscanf(_text, "ServerHost:%[^,],ServerPort:%[0-9],ProxyPort:%[0-9],Password:%[^,]",
		_server_host, _server_port, _listen_port, _password);

	return set_config(_server_host, _server_port, _password, _listen_port);

}

/**
 * ��ӡ������Ϣ
 */
void print_config() {

	char * _datetime = malloc(21);

	datetime(_datetime);

	int pl = strlen(password);

	char _password[pl + 1];

	memset(_password, '*', pl);

	_password[pl] = '\0';

	/**
	 printf("\n[%s] ServerHost��%s\n[%s] ServerPort��%d\n[%s] ProxyPort��%d\n[%s] Password��%s\n",
	 	_datetime, server_host, _datetime, server_port, _datetime, listen_port, _datetime, _password);
	 */
	printf("\n[%s] �ڵ��ַ��%s\n[%s] �ڵ�˿ڣ�%d\n[%s] ����˿ڣ�%d\n[%s] �������룺%s\n",
		_datetime, server_host, _datetime, server_port, _datetime, listen_port, _datetime, _password);

	free(_datetime);

}

/**
 * �ͻ������߳�
 */
void* main_thread() {

	if (load_config() == -1) {

		_log("�޷���ȡ������Ϣ����ֹͣ���в��˳�");

		pthread_exit(0);

	}

	print_config();

	WSADATA wsaData={0};
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
        	_log("Error at WSAStartup()\n");

	int socket_client = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_client == INVALID_SOCKET)
	{
		/*printf("Error at socket(): %ld\n", WSAGetLastError());//�Լ���Ϲ���*/
		_log("�޷����������������ֹͣ���в��˳�");
		pthread_exit(0);
	}

	struct sockaddr_in sockaddr_client;

	memset(&sockaddr_client, 0, sizeof(sockaddr_client));

	sockaddr_client.sin_family = AF_INET;

	sockaddr_client.sin_addr.s_addr = htonl(INADDR_ANY);

	sockaddr_client.sin_port = htons(listen_port);

	BOOL bOptVal = TRUE;
	setsockopt(socket_client, SOL_SOCKET, SO_REUSEADDR, (char *) &bOptVal, sizeof(int));

	int _bind = bind(socket_client, (struct sockaddr *) &sockaddr_client, sizeof(struct sockaddr));

	if (_bind == -1) {

		close(socket_client);

		char message[128];

		sprintf(message, "�󶨶˿�%dʧ�ܣ���ֹͣ���в��˳�", listen_port);

		_log(message);

		pthread_exit(0);

	}

	int _listen = listen(socket_client, 1024);

	if (_listen == -1) {

		close(socket_client);

		char message[128];

		sprintf(message, "�����˿�%dʧ�ܣ���ֹͣ���в��˳�", listen_port);

		_log(message);

		pthread_exit(0);

	}

	for (;;) {

		struct sockaddr_in sockaddr_agent;

		socklen_t sockaddr_size = sizeof(struct sockaddr_in);

		int socket_agent = accept(socket_client, (struct sockaddr *) &sockaddr_agent, &sockaddr_size);

		if (socket_agent == -1) {

			_log("�������������ʱ��������");

			continue;

		}

		pthread_t thread_id;

		pthread_create(&thread_id, NULL, thread_client, (void *) &socket_agent);

	}

	close(socket_client);

	_log("GFW.Press�ͻ������н���");

	pthread_exit(0);

}

pthread_t thread_id;

/**
 * �ͻ���������
 */
int main(int argc, char *argv[]) {

	_log("GFW.Press�ͻ��˿�ʼ����......");
	
	srand( (unsigned)time( NULL ) );
	
	random_load = get_random_load();

	pthread_create(&thread_id, NULL, main_thread, NULL);

	pthread_join(thread_id, NULL);

	/**
	pthread_kill(thread_id, SIGINT);
	*/

	return 0;

}

