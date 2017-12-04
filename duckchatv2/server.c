
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <string>
#include <map>
#include <iostream>
#include <time.h>
#include <set>
#include <string>
using namespace std;
#include "duckchat.h"

#define MAX_CONNECTIONS 10
#define HOSTNAME_MAX 100
#define MAX_MESSAGE_LEN 65536
#define UUID_LENGTH 37

typedef map<string, struct sockaddr_in> channel_type;

int s; //socket for listening
struct sockaddr_in server;
map<string, struct sockaddr_in> usernames;
map<string, int> active_usernames;
map<string, string> rev_usernames;
map<string, channel_type> channels;



map<string, struct sockaddr_in> servers;
map<string, int> channel_subscriptions;
map<string, map<string, struct sockaddr_in>> server_channels;
map<string, pair<string, time_t>> server_timers;

char uuids[65536][UUID_LENGTH];
int timer_flag;

void handle_socket_input();
void handle_login_message(void *data, struct sockaddr_in sock);
void handle_logout_message(struct sockaddr_in sock);
void handle_join_message(void *data, struct sockaddr_in sock);
void handle_leave_message(void *data, struct sockaddr_in sock);


void handle_say_message(void *data, struct sockaddr_in sock);
void handle_list_message(struct sockaddr_in sock);
void handle_who_message(void *data, struct sockaddr_in sock);
void handle_keep_alive_message(struct sockaddr_in sock);
void handle_server_say_message(void *data, struct sockaddr_in sock);
void handle_server_leave_message(void *data, struct sockaddr_in sock);
void handle_server_join_message(void *data, struct sockaddr_in sock);
void send_error_message(struct sockaddr_in sock, string error_msg);
void broadcast_join_message(string origin_server_key, char *channel);
long long uuid_generate()
{
	long long uid = 0LL;
	int fd = open("/dev/urandom", O_RDONLY);

	if (fd == -1)
	{
		return 0;
	}

	int numbytes = read(fd, &uid, 8);

	if (numbytes == 0)
	{
		return 0;
	}

	return uid;
}

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		printf("Usage: ./server domain_name port_num\n");
		exit(1);
	}

	char hostname[HOSTNAME_MAX];
	int port;
	strcpy(hostname, argv[1]);
	port = atoi(argv[2]);
	struct hostent     *he;

	if ((he = gethostbyname(hostname)) == NULL)
	{
		printf("Error resolving hostname\n");
		exit(1);
	}

	struct in_addr **addr_list;

	addr_list = (struct in_addr **) he->h_addr_list;

	char serv_name[HOSTNAME_MAX];

	strcat(serv_name, inet_ntoa(*addr_list[0]));

	strcat(serv_name, ".");

	strcat(serv_name, argv[2]);


	for (int i = 0; i < 65536; i++)
	{
		for (int j = 0; j < 37; j++)
		{
			uuids[i][j] = -1;
		}
	}


	string default_channel = "Common";
	map<string, struct sockaddr_in> default_channel_users;
	channels[default_channel] = default_channel_users;
	channel_subscriptions[default_channel] = 1;
	map<string, struct sockaddr_in> server_names;

	if (argc > 3)
	{
		if (((argc - 3) % 2) != 0)
		{
			printf("Invalid number of arguments.  Usage: ./server domain_name port_num server1_domain server1_port server2_domain server2_port ... serverN_domain serverN_port\n");
		}
		else
		{
			int i = 0;
			do
			{
				if (i % 2 == 0)
				{
					char key[HOSTNAME_MAX + 32] = {0};
					char serv_name[HOSTNAME_MAX + 32] = {0};

					if ((he = gethostbyname(argv[i + 3])) != NULL)
					{
						addr_list = (struct in_addr **) he->h_addr_list;
					strcat(key, inet_ntoa(*addr_list[0]));
					strcat(key, ".");
					strcat(key, argv[i + 4]);
					string server_name = key;
					struct sockaddr_in server_addr;
					servers[server_name] = server_addr;
					servers[server_name].sin_family = AF_INET;
					servers[server_name].sin_port = htons(atoi(argv[i + 4]));
					memcpy(&servers[key].sin_addr, he->h_addr_list[0], he->h_length);
				
					struct sockaddr_in server_addr_cpy;
					server_names[server_name] = server_addr_cpy;
					server_names[server_name].sin_family = AF_INET;
					server_names[server_name].sin_port = htons(atoi(argv[i + 4]));
					memcpy(&server_names[key].sin_addr, he->h_addr_list[0], he->h_length);

					}
					else{
						printf("Error resolving hostname\n");
						exit(1);
					}
					
				}

				
				server_channels[default_channel] = server_names;
				i++;
			}while (i < argc - 3-1);
		}
	}

	s = socket(PF_INET, SOCK_DGRAM, 0);

	if (s > 0)
	{
		server.sin_family = AF_INET;
		server.sin_port = htons(port);

		if ((he = gethostbyname(hostname)) == NULL)
		{
			printf("error resolving hostname..");
			exit(1);
		}

		memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
		int err;
		err = bind(s, (struct sockaddr *)&server, sizeof server);

		if (err < 0)
		{
			perror("bind failed\n");
		}

	
		time_t start_time;
		time(&start_time);

		timer_flag = 0;

		do
		{
			time_t current_time;
			time(&current_time);
			double elapsed_time = (double)difftime(current_time, start_time);

			
			if (elapsed_time < 10) {}
			else
			{

				if (timer_flag != 6)
				{
					timer_flag++;
			
					map<string, struct sockaddr_in>::iterator server_iter = servers.begin();

					while (server_iter != servers.end())
					{
						
						map<string, map<string, struct sockaddr_in>>::iterator channel_iter = server_channels.begin();;

						while (channel_iter != server_channels.end())
						{
							string server_identifier;
							char port_str[6];
							int port = (int)ntohs((server_iter->second).sin_port);
							sprintf(port_str, "%d", port);
							string ip = inet_ntoa((server_iter->second).sin_addr);
							server_identifier = ip + "." + port_str;
							string channel = channel_iter->first;
							map<string, pair<string, time_t>>::iterator timer_iter;

							if ((timer_iter = server_timers.find(server_identifier)) != server_timers.end())
							{
							
								time_t curr_time;
								time(&curr_time);
								double elapsed = (double)difftime(curr_time, (timer_iter->second).second);

								if (elapsed < 120)
								{
								}
								else 
								{
									map<string, struct sockaddr_in>::iterator find_iter;

									if ((find_iter = server_channels[channel].find(server_identifier)) == server_channels[channel].end())
									{
									}
									else
									{
										server_channels[channel].erase(find_iter);
									
										break;
									}

								}
							}

							else
							{
							}

							channel_iter++;
						}

						server_iter++;
					}
				}

				else
				{
					timer_flag = 0;
					
					map<string, struct sockaddr_in>::iterator server_iter = servers.begin();;

					while (server_iter != servers.end())
					{

						map<string, channel_type>::iterator channel_iter = channels.begin();

						while (channel_iter != channels.end())
						{
							struct server_request_join s2s_join;
							ssize_t bytes;
							s2s_join.req_type = SERVER_REQ_JOIN;
							strcpy(s2s_join.req_channel, channel_iter->first.c_str());
							bytes = sendto(s, (void *)&s2s_join, sizeof(s2s_join), 0, (struct sockaddr *) & (server_iter->second), sizeof(server_iter->second));

							if (bytes > 0)
							{
								printf("%s:%i %s:%i send S2S Join refresh\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port), inet_ntoa(server_iter->second.sin_addr), ntohs(server_iter->second.sin_port));
							}

							else
							{
								perror("Message failed");
							}

							channel_iter++;
						}

				
						map<string, map<string, struct sockaddr_in>>::iterator server_channel_iter = server_channels.begin();

						while (server_channel_iter != server_channels.end())
						{
							string server_identifier;
							char port_str[6];
							int port;
							port = (int)ntohs(server_iter->second.sin_port);
							sprintf(port_str, "%d", port);
							strcpy(port_str, port_str);
							string ip = inet_ntoa((server_iter->second).sin_addr);
							server_identifier = ip + "." + port_str;
							string channel = server_channel_iter->first;
							map<string, pair<string, time_t>>::iterator timer_iter;
							timer_iter = server_timers.find(server_identifier);

							if (timer_iter != server_timers.end())
							{
								
								time_t curr_time;
								time(&curr_time);
								double elapsed = (double)difftime(curr_time, (timer_iter->second).second);

								if (elapsed < 120)
								{
								}
								else
								{
									map<string, struct sockaddr_in>::iterator find_iter;

									if ((find_iter = server_channels[channel].find(server_identifier)) == server_channels[channel].end())
									{
									}
									else
									{
										server_channels[channel].erase(find_iter);
										break;
									}
								}
							}

							else
							{
							}

							server_channel_iter++;
						}

						server_iter++;
					}
				}

				time(&start_time);
			}

		
			int rc;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(s, &fds);
			struct timeval max_timeout_interval;
			max_timeout_interval.tv_sec = 10;
			max_timeout_interval.tv_usec = 0;
			rc = select(s + 1, &fds, NULL, NULL, &max_timeout_interval);

			if (rc < 0)
			{
				printf("error in select\n");
				getchar();
			}

			else if (rc == 0)
			{
			
			}
			else
			{
				int socket_data = 0;

				if (FD_ISSET(s, &fds))
				{
					
					handle_socket_input();
					socket_data = 1;
				}
			}
		}
		while (1);

		return 0;
	}

	else
	{
		perror("socket() failed\n");
		exit(1);
	}
}

void handle_socket_input()
{
	struct sockaddr_in recv_client;
	ssize_t bytes;
	void *data;
	size_t len;
	socklen_t fromlen;
	fromlen = sizeof(recv_client);
	char recv_text[MAX_MESSAGE_LEN];
	data = &recv_text;
	len = sizeof recv_text;
	bytes = recvfrom(s, data, len, 0, (struct sockaddr *) &recv_client, &fromlen);

	if (bytes < 0)
	{
		perror("recvfrom failed\n");
	}

	else
	{
		struct request *request_msg;
		request_msg = (struct request *) data;
		request_t message_type = request_msg->req_type;

		if (message_type == REQ_LOGIN)
		{
			handle_login_message(data, recv_client);
		}

		else if (message_type == REQ_LOGOUT)
		{
			handle_logout_message(recv_client);
		}

		else if (message_type == REQ_JOIN)
		{
			handle_join_message(data, recv_client);
		}

		else if (message_type == REQ_LEAVE)
		{
			handle_leave_message(data, recv_client);
		}

		else if (message_type == REQ_SAY)
		{
			handle_say_message(data, recv_client);
		}

		else if (message_type == REQ_LIST)
		{
			handle_list_message(recv_client);
		}

		else if (message_type == REQ_WHO)
		{
			handle_who_message(data, recv_client);
		}

		else if (message_type == 8)
		{
			handle_server_join_message(data, recv_client);
		}

		else if (message_type == 9)
		{
			handle_server_leave_message(data, recv_client);
		}

		else if (message_type == 10)
		{
			handle_server_say_message(data, recv_client);
		}

		else
		{
			send_error_message(recv_client, "*Unknown command");
		}
	}
}
void handle_login_message(void *data, struct sockaddr_in sock)
{
	struct request_login *msg;
	msg = (struct request_login *) data;
	string username = msg->req_username;
	usernames[username]	= sock;
	active_usernames[username] = 1;
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	rev_usernames[key] = username;
	printf("%s:%i %s:%i recv Request Login %s \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str());
}
void handle_logout_message(struct sockaddr_in sock)
{
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	map <string, string> :: iterator iter;
	iter = rev_usernames.find(key);

	if (iter == rev_usernames.end())
	{
		send_error_message(sock, "Not logged in");
	}

	else
	{
		string username = rev_usernames[key];
		rev_usernames.erase(iter);
		map<string, struct sockaddr_in>::iterator user_iter;
		user_iter = usernames.find(username);
		usernames.erase(user_iter);
		map<string, channel_type>::iterator channel_iter;

		for (channel_iter = channels.begin(); channel_iter != channels.end(); channel_iter++)
		{
			map<string, struct sockaddr_in>::iterator within_channel_iterator;
			within_channel_iterator = channel_iter->second.find(username);

			if (within_channel_iterator != channel_iter->second.end())
			{
				channel_iter->second.erase(within_channel_iterator);
			}
		}

		map<string, int>::iterator active_user_iter;
		active_user_iter = active_usernames.find(username);
		active_usernames.erase(active_user_iter);
		printf("%s:%i %s:%i recv Request logout %s \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str());
	}
}
void handle_join_message(void *data, struct sockaddr_in sock)
{
	struct request_join *msg;
	msg = (struct request_join *) data;
	string channel = msg->req_channel;
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	map<string, string>::iterator iter;

	if ((iter = rev_usernames.find(key)) != rev_usernames.end())
	{
		string username = rev_usernames[key];
		map<string, channel_type>::iterator channel_iter;
		active_usernames[username] = 1;

		if ((channel_iter = channels.find(channel)) != channels.end())
		{
			channels[channel][username] = sock;
		}

		else
		{
			map<string, struct sockaddr_in> new_channel_users;
			new_channel_users[username] = sock;
			channels[channel] = new_channel_users;
			channel_subscriptions[channel] = 1;
			char channel_buf[CHANNEL_MAX];
			sprintf(channel_buf, "%s", channel.c_str());
			char port_str[6];
			string domain_name = inet_ntoa(server.sin_addr);
			sprintf(port_str, "%d", (int) ntohs(server.sin_port));
			string origin_server_key = domain_name + "." + port_str;
			map<string, struct sockaddr_in>::iterator server_iterator;
			map<string, struct sockaddr_in> server_names;
			int count = 0;
			server_iterator = servers.begin();

			while (server_iterator != servers.end())
			{
				count++;
				server_names[server_iterator->first] = server_iterator->second;
				server_iterator++;
			}

			server_channels[channel] = server_names;
			printf("%s:%i %s:%i recv Request Join %s %s\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str(), channel.c_str());
			broadcast_join_message(origin_server_key, channel_buf);
		}
	}

	else
	{
		send_error_message(sock, "Not logged in");
	}
}
void handle_leave_message(void *data, struct sockaddr_in sock)
{
	struct request_leave *msg;
	msg = (struct request_leave *) data;
	string channel = msg->req_channel;
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	map <string, string> :: iterator iter;
	iter = rev_usernames.find(key);

	if (iter == rev_usernames.end())
	{
		send_error_message(sock, "Not logged in");
	}

	else
	{
		string username = rev_usernames[key];
		map<string, channel_type>::iterator channel_iter;
		channel_iter = channels.find(channel);
		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			send_error_message(sock, "No channel by the name " + channel);
			printf("%s:%i %s:%i recv Request Leave %s trying to leave non-existent channel %s \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str(), channel.c_str());
		}

		else
		{
			map<string, struct sockaddr_in>::iterator channel_user_iter;
			channel_user_iter = channels[channel].find(username);

			if (channel_user_iter == channels[channel].end())
			{
				send_error_message(sock, "You are not in channel " + channel);
				printf("%s:%i %s:%i recv Request Leave %s trying to leave channel %s where he/she is not a member of\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str(), channel.c_str());
			}

			else
			{
				channels[channel].erase(channel_user_iter);
				printf("%s:%i %s:%i recv Request Leave %s leaves %s\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str(), channel.c_str());

				if (channels[channel].empty() && (channel != "Common"))
				{
					channels.erase(channel_iter);
					channel_subscriptions.erase(channel);
				}
			}
		}
	}
}
void handle_say_message(void *data, struct sockaddr_in sock)
{
	struct request_say *msg;
	msg = (struct request_say *) data;
	string channel = msg->req_channel;
	string text = msg->req_text;
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	map <string, string> :: iterator iter;

	if ((iter = rev_usernames.find(key)) != rev_usernames.end())
	{
		string username = rev_usernames[key];
		map<string, channel_type>::iterator channel_iter;
		active_usernames[username] = 1;
		channel_iter = channels.find(channel);

		if (channel_iter != channels.end())
		{
			map<string, struct sockaddr_in>::iterator channel_user_iter;

			if ((channel_user_iter = channels[channel].find(username)) != channels[channel].end())
			{
				map<string, struct sockaddr_in> existing_channel_users;
				existing_channel_users = channels[channel];
				channel_user_iter = existing_channel_users.begin();

				while (channel_user_iter != existing_channel_users.end())
				{
					ssize_t bytes;
					struct text_say send_msg;
					send_msg.txt_type = TXT_SAY;
					const char *str = channel.c_str();
					sprintf(send_msg.txt_channel, "%s", str);
					str = username.c_str();
					sprintf(send_msg.txt_username, "%s", str);
					str = text.c_str();
					sprintf(send_msg.txt_text, "%s", str);
					struct sockaddr_in send_sock = channel_user_iter->second;
					bytes = sendto(s, (void *)&send_msg, sizeof(send_msg), 0, (struct sockaddr *) &send_sock, sizeof send_sock);

					if (bytes > 0)
					{
					}
					else
					{
						perror("Message failed");
					}

					channel_user_iter++;
				}

				printf("%s:%i %s:%i recv Request Say %s \"%s\"\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), channel.c_str(), text.c_str());
				long long uuid = uuid_generate();
				char uuid_chars[UUID_LENGTH];
				sprintf(uuid_chars, "%llx", uuid);
				int i = 0;

				while (i < 65536)
				{
					if (uuids[i][0] == -1)
					{
						sprintf(uuids[i], "%s", uuid_chars);
						break;
					}

					i++;
				}

				map<string, struct sockaddr_in> existing_channel_servers;
				existing_channel_servers = server_channels[channel];
				map<string, struct sockaddr_in>::iterator server_iter;
				server_iter = existing_channel_servers.begin();

				while (server_iter != existing_channel_servers.end())
				{
					ssize_t bytes;
					struct server_request_say s2s_say;
					s2s_say.req_type = SERVER_REQ_SAY;
					const char *str = channel.c_str();
					sprintf(s2s_say.req_channel, "%s", str);
					str = username.c_str();
					sprintf(s2s_say.req_username, "%s", str);
					str = text.c_str();
					sprintf(s2s_say.req_text, "%s", str);
					int k = 0;

					while (k < 65536)
					{
						if (strcmp(uuids[k], uuid_chars) == 0)
						{
							sprintf(s2s_say.uuid_str, "%s", uuids[k]);
						}

						k++;
					}

					struct sockaddr_in send_sock = server_iter->second;

					bytes = sendto(s, (void *)&s2s_say, sizeof(s2s_say), 0, (struct sockaddr *) &send_sock, sizeof send_sock);

					if (bytes > 0)
					{
						printf("%s:%i %s:%i send S2S Request Say %s %s \"%s\"\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), channel.c_str(), username.c_str(), text.c_str());
					}

					else
					{
						perror("Message failed");
					}

					server_iter++;
				}
			}

			else
			{
				send_error_message(sock, "You are not in channel " + channel);
				printf("%s:%i %s:%i %s trying to send a message to channel %s where he/she is not a member of\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), username.c_str(), channel.c_str());
			}
		}

		else
		{
			send_error_message(sock, "No channel by the name " + channel);
			printf("%s:%i %s:%i %s trying to send message to non-existent channel %s\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), username.c_str(), channel.c_str());
		}
	}

	else
	{
		send_error_message(sock, "Not logged in ");
	}
}
void handle_list_message(struct sockaddr_in sock)
{
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	map <string, string> :: iterator iter;
	iter = rev_usernames.find(key);

	if (iter == rev_usernames.end())
	{
		send_error_message(sock, "Not logged in ");
	}

	else
	{
		string username = rev_usernames[key];
		active_usernames[username] = 1;
		ssize_t bytes;
		size_t len;
		set<string> allChannelsSet;
		map<string, channel_type>::iterator channel_iter;
		set<string>::iterator all_channels_iter;
		map<string, map<string, struct sockaddr_in>>::iterator server_channels_iter;
		int pos = 0;

		for (channel_iter = channels.begin(); channel_iter != channels.end(); channel_iter++)
		{
			allChannelsSet.insert(channel_iter->first);
		}

		for (server_channels_iter = server_channels.begin(); server_channels_iter != server_channels.end(); server_channels_iter++)
		{
			allChannelsSet.insert(server_channels_iter->first);
		}

		int size = allChannelsSet.size();
		struct text_list *send_msg = (struct text_list *) malloc(sizeof(struct text_list) + (size * sizeof(struct channel_info)));
		send_msg->txt_type = TXT_LIST;
		send_msg->txt_nchannels = size;

		for (all_channels_iter = allChannelsSet.begin(); all_channels_iter != allChannelsSet.end(); all_channels_iter++)
		{
			string current_channel = *all_channels_iter;
			const char *str = current_channel.c_str();
			sprintf(((send_msg->txt_channels) + pos)->ch_channel, "%s", str);
			pos++;
		}

		len = sizeof(struct text_list) + (size * sizeof(struct channel_info));
		struct sockaddr_in send_sock = sock;
		bytes = sendto(s, (void *)send_msg, len, 0, (struct sockaddr *) &send_sock, sizeof send_sock);

		if (bytes < 0)
		{
			perror("Message failed\n");
		}

		else
		{
		}

		printf("%s:%i %s:%i recv Request List \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port));
	}
}
void handle_who_message(void *data, struct sockaddr_in sock)
{
	struct request_who *msg;
	msg = (struct request_who *) data;
	string channel = msg->req_channel;
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;
	map <string, string> :: iterator iter;
	iter = rev_usernames.find(key);

	if (iter == rev_usernames.end())
	{
		send_error_message(sock, "Not logged in ");
	}

	else
	{
		string username = rev_usernames[key];
		active_usernames[username] = 1;
		map<string, channel_type>::iterator channel_iter;
		channel_iter = channels.find(channel);

		if (channel_iter == channels.end())
		{
			send_error_message(sock, "No channel by the name " + channel);
			printf("%s:%i %s:%i recv Request Who for non-existing channel \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port));
		}

		else
		{
			map<string, struct sockaddr_in> existing_channel_users;
			existing_channel_users = channels[channel];
			int size = existing_channel_users.size();
			ssize_t bytes;
			void *send_data;
			size_t len;
			struct text_who *send_msg = (struct text_who *) malloc(sizeof(struct text_who) + (size * sizeof(struct user_info)));
			send_msg->txt_type = TXT_WHO;
			send_msg->txt_nusernames = size;
			const char *str = channel.c_str();
			sprintf(send_msg->txt_channel, "%s", str);
			map<string, struct sockaddr_in>::iterator channel_user_iter;
			int pos = 0;

			for (channel_user_iter = existing_channel_users.begin(); channel_user_iter != existing_channel_users.end(); channel_user_iter++)
			{
				string username = channel_user_iter->first;
				str = username.c_str();
				sprintf(((send_msg->txt_users) + pos)->us_username, "%s", str);
				pos++;
			}

			send_data = send_msg;
			len = sizeof(struct text_who) + (size * sizeof(struct user_info));
			struct sockaddr_in send_sock = sock;
			bytes = sendto(s, send_data, len, 0, (struct sockaddr *) &send_sock, sizeof send_sock);

			if (bytes < 0)
			{
				perror("Message failed\n");
			}

			else
			{
			}

			printf("%s:%i %s:%i recv Request Who %s \"%s\"\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int)ntohs(sock.sin_port), username.c_str(), channel.c_str());
		}
	}
}
void handle_server_join_message(void *data, struct sockaddr_in sock)
{
	struct server_request_join *msg;
	msg = (struct server_request_join *)data;
	char channel[CHANNEL_MAX] = {0};
	strcpy(channel, msg->req_channel);
	map<string, int>::iterator subscription_iter = channel_subscriptions.find(channel);
	char port_str[6];
	sprintf(port_str, "%d", htons(sock.sin_port));
	string domain_name = inet_ntoa(sock.sin_addr);
	string origin_server_key = domain_name + "." + port_str;

	if (subscription_iter != channel_subscriptions.end())
	{
		map<string, struct sockaddr_in>::iterator server_iter;
		server_iter = servers.begin();

		while (server_iter != servers.end())
		{
			if (origin_server_key == server_iter->first)
			{
				map<string, pair<string, time_t>>::iterator timer_iter;

				if ((timer_iter = server_timers.find(origin_server_key)) != server_timers.end())
					time(&((timer_iter->second).second));

				else
				{
					time_t new_time;
					time(&new_time);
					pair<string, time_t> channel_time_pair;
					channel_time_pair.first = channel;
					memcpy(&channel_time_pair.second,  &new_time, sizeof new_time);
					server_timers[origin_server_key] = channel_time_pair;
					(server_channels[channel])[server_iter->first] = server_iter->second;
				}
			}

			server_iter++;
		}

		cout << inet_ntoa(server.sin_addr) << ":" << (int)ntohs(server.sin_port)
			 << " " << inet_ntoa(sock.sin_addr) << ":" << (int)ntohs(sock.sin_port)
			 << " recv S2S Join " << channel << " (forwarding ends here)" << endl;
	}

	else
	{
		channel_subscriptions[channel] = 1;
		cout << inet_ntoa(server.sin_addr) << ":" << (int)ntohs(server.sin_port)
			 << " " << inet_ntoa(sock.sin_addr) << ":" << (int)ntohs(sock.sin_port)
			 << " recv S2S Join " << channel <<  endl;
		map<string, struct sockaddr_in>::iterator server_iter;
		map<string, struct sockaddr_in> server_names;
		server_iter = servers.begin();

		while (server_iter != servers.end())
		{
			server_names[server_iter->first] = server_iter->second;
			map<string, pair<string, time_t>>::iterator timer_iter;

			if ((timer_iter = server_timers.find(origin_server_key)) != server_timers.end())
				time(&((timer_iter->second).second));

			else
			{
				//timer not found
				time_t new_time;
				time(&new_time);
				pair<string, time_t> channel_time_pair;
				channel_time_pair.first = channel;
				memcpy(&channel_time_pair.second, &new_time, sizeof new_time);
				server_timers[origin_server_key] = channel_time_pair;
			}

			//end
			server_iter++;
		}

		server_channels[channel] = server_names;
		broadcast_join_message(origin_server_key, channel);
	}
}
void handle_server_leave_message(void *data, struct sockaddr_in sock)
{
	struct server_request_leave *msg;
	msg = (struct server_request_leave *) data;
	string channel = msg->req_channel;
	string ip = inet_ntoa(sock.sin_addr);
	int port = ntohs(sock.sin_port);
	char port_str[6];
	sprintf(port_str, "%d", port);
	string key = ip + "." + port_str;

	map<string, struct sockaddr_in>::iterator server_iter;

	if ((server_iter = server_channels[channel].find(key)) == server_channels[channel].end())
	{
	}
	else
	{
		server_channels[channel].erase(server_iter);
	}

	printf("%s:%i %s:%i recv S2S Leave %s \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), channel.c_str());
}
void handle_server_say_message(void *data, struct sockaddr_in sock)
{
	struct server_request_say *msg;
	msg = (struct server_request_say *)data;
	char channel[CHANNEL_MAX] = {0};
	char username[USERNAME_MAX] = {0};
	char text[SAY_MAX] = {0};
	strcpy(channel, msg->req_channel);
	strcpy(username, msg->req_username);
	strcpy(text, msg->req_text);
	char uuid_chars[37] = {0};
	strcpy(uuid_chars, msg->uuid_str);
	string ip = inet_ntoa(sock.sin_addr);
	char port_str[6];
	sprintf(port_str, "%d", (int)ntohs(sock.sin_port));
	string origin_server = ip + "." + port_str;
	printf("%s:%i %s:%i recv S2S Say %s %s\"%s\"\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), username, channel, text);
	int i = 0;

	while (i < 65536)
	{
		if (uuids[i][0] == -1)
		{
			strcpy(uuids[i], uuid_chars);
			map<string, struct sockaddr_in> existing_channel_users;
			existing_channel_users = channels[channel];
			map<string, struct sockaddr_in>::iterator channel_user_iter = existing_channel_users.begin();

			while (channel_user_iter != existing_channel_users.end())
			{
				ssize_t bytes;
				struct text_say send_msg;
				send_msg.txt_type = TXT_SAY;
				sprintf(send_msg.txt_channel, "%s", channel);
				sprintf(send_msg.txt_username, "%s", username);
				sprintf(send_msg.txt_text, "%s", text);
				struct sockaddr_in send_sock = channel_user_iter->second;
				bytes = sendto(s, (void *)&send_msg, sizeof(send_msg), 0, (struct sockaddr *) &channel_user_iter->second, sizeof channel_user_iter->second);

				if (bytes < 0)
				{
					perror("Message failed");
				}

				channel_user_iter++;
			}

			break;
		}

		else if (strcmp(uuids[i], uuid_chars) == 0)
		{
			map<string, struct sockaddr_in>::iterator server_iter = server_channels[channel].begin();;

			while (server_iter != server_channels[channel].end())
			{
				if (origin_server == server_iter->first)
				{
					ssize_t bytes;
					struct server_request_leave s2s_leave;
					s2s_leave.req_type = SERVER_REQ_LEAVE;
					sprintf(s2s_leave.req_channel, "%s", channel);
					bytes = sendto(s, (void *)&s2s_leave, sizeof(s2s_leave), 0, (struct sockaddr *)(&server_iter->second), sizeof(server_iter->second));

					if (bytes < 0)
					{
						perror("Message failed");
					}

					else
					{
						printf("%s:%i %s:%i send S2S Leave \n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port));
					}

					break;
				}

				server_iter++;
			}

			return;
		}

		else
		{
		}

		i++;
	}

	int server_subscribers = 0;
	map<string, struct sockaddr_in>::iterator server_iter = server_channels[channel].begin();;

	while (server_iter != server_channels[channel].end())
	{
		if (origin_server == server_iter->first)
		{
		}
		else
		{
			server_subscribers++;
		}

		server_iter++;
	}

	if (server_subscribers <= 0)
	{
		if (channels[channel].empty() && (channel != "Common"))
		{
			ssize_t bytes;
			struct server_request_leave s2s_leave;
			s2s_leave.req_type = SERVER_REQ_LEAVE;
			sprintf(s2s_leave.req_channel, "%s", channel);
			bytes = sendto(s, (void *) &s2s_leave, sizeof(s2s_leave), 0, (struct sockaddr *) &sock, sizeof sock);

			if (bytes < 0)
			{
				perror("Message failed");
			}

			else
			{
				printf("%s:%i %s:%i send S2S Leave\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port));
			}
		}
	}

	else
	{
		ssize_t bytes;
		struct server_request_say s2s_say;
		s2s_say.req_type = SERVER_REQ_SAY;
		strcpy(s2s_say.req_username, username);
		strcpy(s2s_say.req_channel, channel);
		strcpy(s2s_say.req_text, text);
		int k = 0;

		while (k < 65536)
		{
			if (strcmp(uuid_chars, uuids[k]) != 0)
			{
			}
			else
			{
				strcpy(s2s_say.uuid_str, uuids[k]);
			}

			k++;
		}

		server_iter = server_channels[channel].begin();

		while (server_iter != server_channels[channel].end())
		{
			if (origin_server == server_iter->first)
			{}
			else
			{
				bytes = sendto(s, (void *)&s2s_say, sizeof(s2s_say), 0, (struct sockaddr *)&server_iter->second, sizeof server_iter->second);

				if (bytes < 0)
				{
					perror("Message failed");
				}

				else
				{
					printf("%s:%i %s:%i recv S2S Say %s %s\"%s\"\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(sock.sin_addr), (int) ntohs(sock.sin_port), username, channel, text);
				}

				server_iter++;
			}
		}
	}
}
void broadcast_join_message(string origin_server_key, char *channel)
{
	ssize_t bytes;
	map<string, struct sockaddr_in>::iterator server_iter;
	struct server_request_join join_msg;
	join_msg.req_type = SERVER_REQ_JOIN;
	sprintf(join_msg.req_channel, "%s", channel);
	server_iter = servers.begin();

	while (server_iter != servers.end())
	{
		if (server_iter->first == origin_server_key)
		{
		}
		else
		{
			bytes = sendto(s, (void *) &join_msg, sizeof(join_msg), 0,
						   (struct sockaddr *) &server_iter->second, sizeof server_iter->second);

			if (bytes > 0)
			{
				printf("%s:%i %s:%i send S2S Request Join %s\n", inet_ntoa(server.sin_addr), (int) ntohs(server.sin_port), inet_ntoa(server_iter->second.sin_addr), (int) ntohs(server_iter->second.sin_port), channel);
			}

			else
			{
				perror("Message failed\n");
			}
		}

		server_iter++;
	}
}
void send_error_message(struct sockaddr_in sock, string error_msg)
{
	ssize_t bytes;
	void *send_data;
	size_t len;
	struct text_error send_msg;
	send_msg.txt_type = TXT_ERROR;
	const char *str = error_msg.c_str();
	sprintf(send_msg.txt_error, "%s", str);
	send_data = &send_msg;
	len = sizeof send_msg;
	struct sockaddr_in send_sock = sock;
	bytes = sendto(s, send_data, len, 0, (struct sockaddr *) &send_sock, sizeof send_sock);

	if (bytes < 0)
	{
		perror("Message failed\n");    //error
	}
}
