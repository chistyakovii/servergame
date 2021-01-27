#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <math.h>

#define BUF_S 1024
#define READBUF_S 100
#define PROCESS 1
#define WAIT 0
#define READY 1
#define NOTREADY 0
#define INGAME 0
#define OUTGAME 1
#define MONTH_BUILD 4
#define MAX_BUILD_FACT 150

enum modes {
	DESCENDING=0,
	ASCENDING=1,
	BUY=0,
	SELL=1,
};

enum start_parameters {
	START_FACT=2,
	START_PROD=2,
	START_MAT=4,
	START_MONEY=10000,
	START_AUCTION=0,
};

enum costs {
	COST_PROD=500,
	COST_MAT=300,
	COST_FACT=1000,
	HPRICE_FACT=2500,
};

const int level_change[5][5]= {
	{4,4,2,1,1},
	{3,4,3,1,1},
	{1,3,4,3,1},
	{1,1,3,4,3},
	{1,1,2,4,4},
};

const float market_level[5][4]= {
	{1.0,800,3.0,6500},
	{1.5,650,2.5,6000},
	{2.0,500,2.0,5500},
	{2.5,400,1.5,5000},
	{3.0,300,1.0,4500},
};

static const char help[]=
			"=================Help=================\r\n\n"
			"market - get information about the market\r\n\n"
			"player <playernumber> - "
			"get information about player " 
			" with this or that number\r\n\n"
			"myplayer - know your player`s number\r\n\n"
			"prod - make 1 product on factory "
			"you should have 1 material and 2000 money\r\n\n"
			"buy <quantity_mat> <what_price> -\r\n"
		        "application for participation in "
			"the auction of materials\r\n\n"
			"sell <quantity_products> <what_price> -\r\n"
		        "application for participation in "
			"the auction of products\r\n\n"
			"build - application for building the facroty\r\n\n"
			"turn - finish your move\r\n\n"
			"=================Help=================\r\n\0";

static const char hello[]="=======Welcome to the server=======\r\n\0";

static const char start_of_game[]=
			"=============Server=============\r\n"
			"	The game starts\r\n"
			"================================\r\n\0";

static const char started[]=
			"=============Server=============\r\n"
			"The game had already started\r\n"
			"================================\r\n\0";

static const char not_started[]=
			"=============Server=============\r\n"
			"The game hadn`t started yet\r\n"
			"================================\r\n\0";

static const char unknow_command[]=
			"======================================\r\n\n"
				"Unknow command: "
				"for more information dial 'help'\r\n\n"
			"======================================\r\n\0";

static const char move_over[]=
			"=============Server=============\r\n"
			"       Your move is over\r\n"
			"=============Server=============\r\n\0";

static char not_enough_money[]=
			"=============Server=============\r\n"
			"  You don`t have enough money\r\n"
			"================================\r\n\0";
static char not_enough_prod[]=
			"=============Server=============\r\n"
			"You don`t have enough products\r\n"
			"================================\r\n\0";
static char not_enough_mat[]=
			"=============Server=============\r\n"
			"You don`t have enough materials\r\n"
			"================================\r\n\0";
static char not_enough_fact[]=
			"=============Server=============\r\n"
			"You don`t have enough factories\r\n"
			"================================\r\n\0";
static char applic_accepted[]=
			"=============Server=============\r\n"
			"   Your application accepted\r\n"
			"================================\r\n\0";
static char alr_applied[]=
			"=============Server=============\r\n"
			"   You have already applied\r\n"
		        "	   application\r\n"
			"================================\r\n\0";
static char low_price[]=
			"=============Server=============\r\n"
			"Too low price, see market prices\r\n"
			"================================\r\n\0";
static char high_price[]=
			"=============Server=============\r\n"
			"Too high price, see market prices\r\n"
			"================================\r\n\0";
static char much_materials[]=
			"=============Server=============\r\n"
			"Too much materials, see market items\r\n"
			"================================\r\n\0";
static char much_products[]=
			"=============Server=============\r\n"
			"Too much products, see market items\r\n"
			"================================\r\n\0";
static char new_month[]=
			"=============Server=============\r\n"
			"       New month started\r\n"
			"================================\r\n\0";
static char not_succ_bauct[]=
			"=============Server=============\r\n"
			"    Your buy materials auction\r\n" 
			"    application is not satisfied\r\n"
			"================================\r\n\0";
static char not_succ_sauct[]=
			"=============Server=============\r\n"
			"    Your sell products auction\r\n" 
			"   application is not satisfied\r\n"
			"================================\r\n\0";
static char pl_not_found[]=
			"=============Server=============\r\n"
			"Player with this number not found\r\n"
			"================================\r\n\0";
static char too_much_build_fact[]=
			"=============Server=============\r\n"
			"   Too much building factories\r\n"
			"================================\r\n\0";
static char win[]=
			"=============Server=============\r\n"
			"$$$$$$$$$$$$$You win$$$$$$$$$$$$$\r\n" 
			"$$$$$$$$$Congratulations$$$$$$$$$\r\n"
			"================================\r\n\0";
static char lose[]=
			"=============Server=============\r\n"
			"************You lose************\r\n"
			" Wait for the end of the game or\r\n" 
			"*********leave the game*********\r\n"
			"================================\r\n\0";

struct item {
	int symb;
	struct item *next;
};

void free_item(struct item **word_list)
{		
	struct item *word_tmp;
	while(*word_list!=NULL) {
		word_tmp=*word_list;
		*word_list=(*word_list)->next;
		free(word_tmp);
	}
}

struct server_cmd {
	int port;
	int max_players;
	int quant_players;
	int active_players;
	int flag_start;
	int month;
	int market_level;
	int quant_prod;
	int quant_mat;
	int min_mat_price;
	int max_prod_price;

};

void set_cmd_start_parameters(struct server_cmd *cmd)
{
	cmd->active_players=cmd->max_players;
	cmd->month=1;
	cmd->market_level=3;
}

void set_market_specifications(struct server_cmd *cmd,int active_players)
{
	cmd->quant_mat=
		(int)(market_level[cmd->market_level-1][0]*active_players);
	cmd->min_mat_price=(int)market_level[cmd->market_level-1][1];
	cmd->quant_prod=
		(int)(market_level[cmd->market_level-1][2]*active_players);
	cmd->max_prod_price=(int)market_level[cmd->market_level-1][3];
	/*printf("quant_mat=%d\nmin_price=%d\nquant_prod=%d\nmax_price=%d\n",
			cmd.quant_mat,
			cmd.min_mat_price,
			cmd.quant_prod,
			cmd.max_prod_price);*/
}

void set_cmd(struct server_cmd *cmd,char **argv) 
{
	if(argv[1]!=NULL&&argv[2]!=NULL&&argv[3]==NULL) {
		cmd->port=atoi(argv[1]);
		cmd->max_players=atoi(argv[2]);
	} else {
		printf("%s",argv[3]);
		printf("Bad command arguments\n");
		exit(1);
	}
	cmd->flag_start=WAIT;
	cmd->quant_players=0;
	set_cmd_start_parameters(cmd);
	set_market_specifications(cmd,cmd->max_players);

}


struct words {
	char *word;
	struct words *next;
};

void free_words(struct words **list)
{
	struct words *tmp;
	while(*list!=NULL) {
		tmp=*list;
		*list=(*list)->next;
		free(tmp->word);
		free(tmp);
	}
}

void output_list(struct words *list)
{
	while(list!=NULL) {
		printf("%s\n",list->word);
		list=list->next;
	}
}	

struct clients {
	int client;
	char buf[BUF_S];
	int index;
	int player_number;
	int quant_fact;
	int quant_mat;
	int quant_prod;
	int money;
	int flag_move;
	int flag_status;
	int sell_price;
	int buy_price;
	int buy_mat;
	int sell_prod;
	int make_products;
	int build_fact[MAX_BUILD_FACT];
	struct clients *next;
};

void client_print(struct clients *cl_list)
{
	struct clients *cl_tmp=cl_list;
	while(cl_tmp!=NULL) {
		printf(
			"=========Player number #%d========\r\n"
			"Quantity Factories: %d\r\n"
			"Quantity Materials: %d\r\n"
			"Quantity Products: %d\r\n"
			"Quantity Money: %d\r\n"
			"Game status: %d\r\n"
			"Sell Price: %d\r\n"
			"Sell Products: %d\r\n"
			"Buy Price: %d\r\n"
			"Buy Materials: %d\r\n"
			"Make Products: %d\r\n"
			"===================================\r\n",
			cl_tmp->player_number,
			cl_tmp->quant_fact,
			cl_tmp->quant_mat,
			cl_tmp->quant_prod,
			cl_tmp->money,
			cl_tmp->flag_status,
			cl_tmp->sell_price,
			cl_tmp->sell_prod,
			cl_tmp->buy_price,
			cl_tmp->buy_mat,
			cl_tmp->make_products);
		cl_tmp=cl_tmp->next;
	}
}

void output_clients(struct clients *list)
{
	printf("===List===\n");
	while(list!=NULL) {
		printf("Client number=%d\n",list->client);
		printf(list->buf);
		printf("\n");
		list=list->next;
	}
	printf("===List===\n");
}

void set_build_fact(int *build_fact)
{
	int i;
	for(i=0;i<MAX_BUILD_FACT;i++) {
		build_fact[i]=-1;
	}
}

void set_cl_start_parameters(struct clients *cl_tmp) 
{
	cl_tmp->index=0;
	cl_tmp->quant_fact=START_FACT;
	cl_tmp->quant_mat=START_MAT;
	cl_tmp->quant_prod=START_PROD;
	cl_tmp->money=START_MONEY;
	cl_tmp->flag_move=NOTREADY;
	cl_tmp->flag_status=INGAME;
	cl_tmp->sell_price=START_AUCTION;
	cl_tmp->buy_price=START_AUCTION;
	cl_tmp->buy_mat=START_AUCTION;
	cl_tmp->sell_prod=START_AUCTION;
	cl_tmp->make_products=0;
	set_build_fact(cl_tmp->build_fact);
}

void add_link_clients(struct clients **cl_list,int client)
{
	struct clients *cl_tmp;
	if(*cl_list==NULL) {
		*cl_list=malloc(sizeof(struct clients));
		cl_tmp=*cl_list;
	} else {
		cl_tmp=*cl_list;
		while(cl_tmp->next!=NULL) {
			cl_tmp=cl_tmp->next;
		}
		cl_tmp->next=malloc(sizeof(struct clients));
		cl_tmp=cl_tmp->next;
	}
	cl_tmp->client=client;
	set_cl_start_parameters(cl_tmp);
	cl_tmp->next=NULL;
}


void free_clients(struct clients **list)
{
	struct clients *tmp;
	while(*list!=NULL) {
		tmp=*list;
		*list=(*list)->next;
		free(tmp);
	}
}

int create_socket()
{
	int ls, opt=1;
	ls=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(ls==-1) {
		perror("socket");
		fflush(stderr);
		exit(1);
	}
	setsockopt(ls,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	return ls;
}

void link_socket_addr(int port,int ls)
{
	struct sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(port);
	addr.sin_addr.s_addr=INADDR_ANY;
	if(bind(ls,(struct sockaddr *) &addr,sizeof(addr))!=0) {
		perror("bind");
		fflush(stderr);
		exit(1);
	}
}

int set_listening_socket(struct server_cmd cmd) 
{
	int ls;
	ls=create_socket();
	link_socket_addr(cmd.port,ls);
	if(listen(ls,5)==-1) {
		perror("listen");
		fflush(stderr);
		exit(1);
	}
	return ls;
}

void quant_pl_distribution(struct clients *cl_list,struct server_cmd cmd)
{
	char buf[150];
	int client;
	while(cl_list!=NULL) {
		client=cl_list->client;
		sprintf(buf,
			"=============Server=============\r\n"
			"   Quantity of players=%d\r\n"
			"================================\r\n",
			cmd.quant_players);
		write(client,buf,strlen(buf)+1);
		if(cmd.flag_start==WAIT) {
			sprintf(buf,
				"=============Server=============\r\n"
				"   Waiting for %d players\r\n"
				"===============================\r\n",
					cmd.max_players-cmd.quant_players);

			write(client,buf,strlen(buf)+1);
		}
		cl_list=cl_list->next;
	}
}

void start_pl_distribution(struct clients *cl_list)
{
	int client;
	while(cl_list!=NULL) {
		client=cl_list->client;
		write(client,start_of_game,strlen(start_of_game)+1);
		cl_list=cl_list->next;
	}
}

fd_set fill_fd_set(struct clients *cl_list,int *max_d,int ls)
{
	int client;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(ls,&readfds);
	*max_d=ls;
	while(cl_list!=NULL) {
		client=cl_list->client;
		FD_SET(client,&readfds);
		if(client>*max_d) {
			*max_d=client;
		}
		cl_list=cl_list->next;
	}
	return readfds;
}

void correct_list(struct clients **cl_list,int client)
{
	struct clients *cl_tmp;
	struct clients **tmp;
	shutdown(client,2);
	close(client);
	tmp=&(*cl_list);
	while(*tmp!=NULL) {
		if((*tmp)->client==client) {
			cl_tmp=*tmp;
			*tmp=(*tmp)->next;
			free(cl_tmp);
		} else {
			tmp=&(*tmp)->next;
		}
	}
}

int check_create_list(int symb,int flag_word)
{
	if(symb==' '||symb=='\t'||symb=='\n') {
		if(flag_word==1) {
			return 0;
		}
	}
	return -1;
}

int check_word(int symb)
{
	if(symb!=' '&&symb!='\n'&&symb!='\t') {
			return 0;	
	}
	return -1;
}

void read_word(struct item **word_list,int symb)
{
	static struct item *tmp;
	if(*word_list==NULL) {
		*word_list=malloc(sizeof(struct item));
		tmp=*word_list;
	} else {
		tmp->next=malloc(sizeof(struct item));
		tmp=tmp->next;
	}
	tmp->symb=symb;
	tmp->next=NULL;
}

int word_length(struct item *word_list)
{
	int i=0;
	while(word_list!=NULL) {
		i++;
		word_list=word_list->next;
	}
	return i;
}

char *fill_word(struct item *word_list)
{
	int k=0;
	char *word;
	word=malloc(word_length(word_list)+1);
	while(word_list!=NULL) {
		word[k]=word_list->symb;
		word_list=word_list->next;
		k++;
	}
	word[k]='\0';
	return word;
}

void create_list(struct words **cmd_list,struct item *word_list)
{
	static struct words *tmp;
	if(*cmd_list==NULL) {
		*cmd_list=malloc(sizeof(struct words));
		tmp=*cmd_list;
	} else {
		tmp->next=malloc(sizeof(struct words));
		tmp=tmp->next;
	}
	tmp->word=fill_word(word_list);
	tmp->next=NULL;
}

struct words *create_cmd_list(char *buf)
{
	int i=0, symb, flag_word=0;
	struct item *word_list=NULL;
	struct words *cmd_list=NULL;
	while(buf[i]!='\0') {
		symb=buf[i];
		if(check_word(symb)==0) {
			read_word(&word_list,symb);
			flag_word=1;
		}
		if(check_create_list(symb,flag_word)==0) {
			create_list(&cmd_list,word_list);
			flag_word=0;
			free_item(&word_list);
		}
		i++;
	}
	return cmd_list;
}

int check_player_cmd(struct words *cmd_list) 
{
	if(cmd_list!=NULL) {
		if(strcmp(cmd_list->word,"player")==0) {
			cmd_list=cmd_list->next;
			if(cmd_list!=NULL&&cmd_list->next==NULL) 
				return 0;
		}
	}
	return -1;
}
	
int check_buy_sell_cmd(struct words *cmd_list,char buffer[])
{
	if(cmd_list!=NULL) {
		if(strcmp(cmd_list->word,buffer)==0) {
			cmd_list=cmd_list->next;
			if(cmd_list!=NULL&&cmd_list->next!=NULL) {
				if(cmd_list->next->next==NULL) {
					return 0;
				}
			}
		}
	}
	return -1;
}

void buy_materials(struct words *cmd_list,struct clients *cl_tmp,
			struct server_cmd *cmd)
{
	int client=cl_tmp->client;
	cmd_list=cmd_list->next;
	char how_much[50];
	char what_price[50];
	sprintf(how_much,cmd_list->word);
	sprintf(what_price,cmd_list->next->word);
	int price=atoi(what_price);
	int quantity=atoi(how_much);
	if(cmd->quant_mat<quantity) {
		write(client,much_materials,strlen(much_materials)+1);
		return;
	}
	if(cmd->min_mat_price>price) {
		write(client,low_price,strlen(low_price)+1);
		return;
	}
	if(price*quantity>cl_tmp->money) {
		write(client,not_enough_money,strlen(not_enough_money)+1);
	} else { 
		cl_tmp->buy_price=price;
		cl_tmp->buy_mat=quantity;
		cl_tmp->money=cl_tmp->money-(price*quantity);
		write(client,applic_accepted,strlen(applic_accepted)+1);
	}
	//printf("%d\n",cl_tmp->buy_mat);
	//printf("%d\n",cl_tmp->b_deposit);
}

void sell_products(struct words *cmd_list,struct clients *cl_tmp,
			struct server_cmd *cmd)
{
	int client=cl_tmp->client;
	cmd_list=cmd_list->next;
	char how_much[50];
	char what_price[50];
	sprintf(how_much,cmd_list->word);
	sprintf(what_price,cmd_list->next->word);
	int price=atoi(what_price);
	int quantity=atoi(how_much);
	if(cmd->quant_prod<quantity) {
		write(client,much_products,strlen(much_products)+1);
		return;
	}
	if(cmd->max_prod_price<price) {
		write(client,high_price,strlen(high_price)+1);
		return;
	}
	if(quantity>cl_tmp->quant_prod) {
		write(client,not_enough_prod,strlen(not_enough_prod)+1);
	} else { 
		cl_tmp->sell_price=price;
		cl_tmp->sell_prod=quantity;
		cl_tmp->quant_prod=cl_tmp->quant_prod-quantity;
		write(client,applic_accepted,strlen(applic_accepted)+1);
	}
	//printf("%d\n",(*cl_list)->sell_prod);
	//printf("%d\n",(*cl_list)->sell_price);
}

int check_cmd(struct words *cmd_list,char buffer[]) 
{
	if(cmd_list!=NULL) {
		if(strcmp(cmd_list->word,buffer)==0) {
			if(cmd_list->next==NULL) 
				return 0;
		}
	}
	return -1;
}

void market_cmd(struct words *cmd_list,struct clients *cl_tmp,
						struct server_cmd *cmd) 
{	
	char market[500];
	int client=cl_tmp->client;
	sprintf(market,
			"=============Market=============\r\n"
			"Current month is %dth\r\n"
			"Players still active:\r\n"
			"$ 	%d\r\n"
			"Bank sells:  items min.price\r\n"
			"$	      %d     %d\r\n"
			"Bank buys:   items max.price\r\n"
			"&	      %d     %d\r\n"
			"================================\r\n",
			cmd->month,
			cmd->active_players,
			cmd->quant_mat,cmd->min_mat_price,
			cmd->quant_prod,cmd->max_prod_price);
	write(client,market,strlen(market)+1);
}

void prod_cmd(struct clients *cl_tmp)
{
	int client=cl_tmp->client;
	int money=cl_tmp->money;
	int quant_mat=cl_tmp->quant_mat;
	int make_products=cl_tmp->make_products;
	if(money>=2000&&quant_mat>0&&cl_tmp->quant_fact>0) {
		cl_tmp->make_products=make_products+1;
		cl_tmp->money=money-2000;
		cl_tmp->quant_mat=quant_mat-1;
		cl_tmp->quant_fact--;
	} else {
		if(money<2000) {
			write(client,not_enough_money,
					strlen(not_enough_money)+1);
		}
		if(quant_mat<1) {
			write(client,not_enough_mat,strlen(not_enough_mat)+1);
		}
		if(cl_tmp->quant_fact<1) {
			write(client,not_enough_fact,strlen(not_enough_fact)+1);
		}
	}
	//printf("%d\n",cl_tmp->quant_mat);
	//printf("%d\n",cl_tmp->make_products);
}

int return_quant_build_fact(struct clients *cl_tmp) 
{
	int i, quantity=0;
	for(i=0;i<MAX_BUILD_FACT;i++) {
		if(cl_tmp->build_fact[i]!=-1) {
			quantity++;
		}
	}
	return quantity;
}
			

void player_cmd(struct words *cmd_list,struct clients *cl_tmp,
						struct clients *cl_list)
{
	char player_inf[300];
	int player_number, present=0, client=cl_tmp->client;
	cmd_list=cmd_list->next;
	player_number=atoi(cmd_list->word);
	while(cl_list!=NULL) {
		if(cl_list->player_number==player_number) {
			int quant_build_fact=return_quant_build_fact(cl_list);
			sprintf(player_inf,
				"========Player #%d ========\r\n"
				"Not Working Factories:   %d\r\n"
				"Working Factories:	%d\r\n"
				"Building Factories:	%d\r\n"
				"Products:    %d\r\n"
				"Materials:   %d\r\n"
				"Money:       %d\r\n"
				"Move status:	%d\r\n"
				"1 - Finished move\r\n"
			        "0 - Making move\r\n" 
				"==========================\r\n",
				cl_list->player_number,
				cl_list->quant_fact,
				cl_list->make_products,
				quant_build_fact,
				cl_list->quant_prod,
				cl_list->quant_mat,
				cl_list->money,
				cl_list->flag_move);
			write(client,player_inf,strlen(player_inf)+1);
			present=1;
			break;
		}
		cl_list=cl_list->next;
	}
	if(present==0) {
		write(client,pl_not_found,strlen(pl_not_found)+1);
	}
}

void myplayer_cmd(struct clients *cl_tmp)
{
	int client=cl_tmp->client;
	char buf[100];
	sprintf(buf,
			"=============Server=============\r\n"
			"     Your player number %d\r\n"
			"================================\r\n",
			cl_tmp->player_number);
	write(client,buf,strlen(buf)+1);
}

void build_cmd(struct clients *cl_tmp)
{
	int i, client=cl_tmp->client;
	if(cl_tmp->money<HPRICE_FACT) {
		write(client,not_enough_money,strlen(not_enough_money)+1);
		return;
	}
	for(i=0;i<MAX_BUILD_FACT;i++) {
		if(cl_tmp->build_fact[i]==-1) {
			cl_tmp->money=cl_tmp->money-HPRICE_FACT;
			cl_tmp->build_fact[i]=MONTH_BUILD;
			write(client,applic_accepted,
					strlen(applic_accepted)+1);
			return;
		}
	}
	write(client,too_much_build_fact,strlen(too_much_build_fact)+1);
}

int cmd_always_use(struct words *cmd_list,struct clients *cl_tmp,
			struct clients *cl_list,struct server_cmd *cmd)
{
	int client=cl_tmp->client;
	if(check_player_cmd(cmd_list)==0) {
		player_cmd(cmd_list,cl_tmp,cl_list);
		return 0;
	}
	if(check_cmd(cmd_list,"market")==0) {
		market_cmd(cmd_list,cl_tmp,cmd);
		return 0;
	}
	if(check_cmd(cmd_list,"myplayer")==0) {
		myplayer_cmd(cl_tmp);
		return 0;
	}
	if(check_cmd(cmd_list,"help")==0) {
		write(client,help,strlen(help)+1);
		return 0;
	}
	return -1;
}

int cmd_move_use(struct words *cmd_list,struct clients *cl_tmp,
				struct server_cmd *cmd)
{
	int client=cl_tmp->client;
	if(check_cmd(cmd_list,"turn")==0) {
		cl_tmp->flag_move=READY;
		return 0;
	}
	if(check_cmd(cmd_list,"prod")==0) {
		prod_cmd(cl_tmp);
		return 0;
	}
	if(check_cmd(cmd_list,"build")==0) {
		build_cmd(cl_tmp);
		return 0;
	}
	if(check_buy_sell_cmd(cmd_list,"buy")==0) {
		if(cl_tmp->buy_price==0) {
			buy_materials(cmd_list,cl_tmp,cmd);
		} else {
			write(client,alr_applied,
					strlen(alr_applied)+1);
		}
		return 0;
	}
	if(check_buy_sell_cmd(cmd_list,"sell")==0) {
		if(cl_tmp->sell_price==0) {
			sell_products(cmd_list,cl_tmp,cmd);
		} else {
			write(client,alr_applied,
					strlen(alr_applied)+1);
		}
		return 0;
	}
	return -1;
}

void client_command(struct clients *cl_tmp,struct clients *cl_list,
						struct server_cmd *cmd)
{
	struct words *cmd_list=create_cmd_list(cl_tmp->buf);
	int client=cl_tmp->client;
	if(cmd->flag_start==PROCESS) {
		if(cmd_always_use(cmd_list,cl_tmp,cl_list,cmd)==0) {
			return;
		}
		if(cl_tmp->flag_move==NOTREADY&&cl_tmp->flag_status!=OUTGAME) {
			if(cmd_move_use(cmd_list,cl_tmp,cmd)==0) {
				return;
			}
			write(client,unknow_command,strlen(unknow_command)+1);
		} else {
			write(client,move_over,strlen(move_over)+1);
		}
	} else {
		write(client,not_started,strlen(not_started)+1);
	}
	free_words(&cmd_list);
}

int buf_processing(int rb,struct clients *cl_tmp,char *readbuf)
{
	int i, index=cl_tmp->index;
	for(i=0;i<rb;i++) {
		if(readbuf[i]!='\n') {
			if(readbuf[i]!='\r') {
				cl_tmp->buf[index]=readbuf[i];
				index++;
				cl_tmp->index++;
			}
		} else {
			cl_tmp->buf[index]=readbuf[i];
			index++;
			cl_tmp->buf[index]='\0';
			cl_tmp->index=0;
			return 0;
		}
	}
	return -1;
}

int client_service(struct clients *cl_tmp,struct clients *cl_list,
			struct server_cmd *cmd)
{
	char readbuf[READBUF_S];
	int rb, client=cl_tmp->client;
	rb=read(client,readbuf,sizeof(readbuf)-1);
	readbuf[rb]='\0';
	if(buf_processing(rb,cl_tmp,readbuf)==0) {
		client_command(cl_tmp,cl_list,cmd);
	}
	if(rb==0) {
		return -1;
	}
	return 0;
}

void work_with_clients(struct clients **cl_list,fd_set readfds,
					struct server_cmd *cmd)
{
	int client;
	struct clients *cl_tmp=*cl_list;
	struct clients *cl_q=*cl_list;
	while(cl_tmp!=NULL) {
		client=cl_tmp->client;
		if(FD_ISSET(client,&readfds)) {
			if(client_service(cl_tmp,cl_q,cmd)==-1) {
				cmd->quant_players--;
				correct_list(cl_list,client);
				output_clients(*cl_list);
				cl_tmp=*cl_list;
				quant_pl_distribution(*cl_list,*cmd);
			}
		}
		cl_tmp=cl_tmp->next;
	}
}

void give_clients_numbers(struct clients *cl_list)
{
	int numb=1;
	while(cl_list!=NULL) {
		cl_list->player_number=numb;
		cl_list=cl_list->next;
		numb++;
	}
}

void break_connection(int client)
{
	write(client,started,strlen(started)+1);
	shutdown(client,2);
	close(client);
}

void accept_client(struct server_cmd *cmd,struct clients **cl_list,
								int client)
{
	cmd->quant_players++;
	write(client,hello,strlen(hello)+1);
	add_link_clients(cl_list,client);
	quant_pl_distribution(*cl_list,*cmd);
}

void new_clients(struct clients **cl_list,int ls,struct server_cmd *cmd)
{
	int client;
	client=accept(ls,NULL,NULL);
	if(client!=-1) {
		if(cmd->flag_start!=PROCESS
				&&cmd->quant_players!=cmd->max_players) {
			accept_client(cmd,cl_list,client);
		} else {
			break_connection(client);
		}
		if(cmd->flag_start!=PROCESS
				&&cmd->quant_players==cmd->max_players) {
			start_pl_distribution(*cl_list);
			give_clients_numbers(*cl_list);
			cmd->flag_start=PROCESS;
		}
	} else {
		perror("accept");
		fflush(stderr);
	}
}

void work_with_build_fact(struct clients *cl_tmp)
{
	int i;
	for(i=0;i<MAX_BUILD_FACT;i++) {
		if(cl_tmp->build_fact[i]!=-1) {
			if(cl_tmp->build_fact[i]==0) {
				cl_tmp->quant_fact++;
				cl_tmp->money=cl_tmp->money-HPRICE_FACT;
			}
			cl_tmp->build_fact[i]--;
		}
	}
}

void work_with_player(struct clients *cl_tmp)
{
	if(cl_tmp->make_products!=0) {
		cl_tmp->quant_prod=cl_tmp->quant_prod+cl_tmp->make_products;
		cl_tmp->quant_fact=cl_tmp->quant_fact+cl_tmp->make_products;
		cl_tmp->make_products=0;
	}
	cl_tmp->flag_move=NOTREADY;
	cl_tmp->money=cl_tmp->money-(cl_tmp->quant_prod*COST_PROD);
	cl_tmp->money=cl_tmp->money-(cl_tmp->quant_fact*COST_FACT);
	cl_tmp->money=cl_tmp->money-(cl_tmp->quant_mat*COST_MAT);
	cl_tmp->buy_price=START_AUCTION;
	cl_tmp->buy_mat=START_AUCTION;
	cl_tmp->sell_price=START_AUCTION;
	cl_tmp->sell_prod=START_AUCTION;
	work_with_build_fact(cl_tmp);
}

void work_with_players(struct clients *cl_list) 
{
	while(cl_list!=NULL) {
		if(cl_list->flag_status!=OUTGAME) {
			int client=cl_list->client;
			work_with_player(cl_list);
			if(cl_list->money<=0) {
				cl_list->flag_status=OUTGAME;
				write(client,lose,strlen(lose)+1);
			}
			write(client,new_month,strlen(new_month)+1);
		}
		cl_list=cl_list->next;
	}
}

void change_market_level(struct server_cmd *cmd)
{
	int r, sum=0, new_level=0;
	r=1+(int)(12.0*rand()/(RAND_MAX+1.0));
	while(r>sum) {
		sum=sum+level_change[cmd->market_level-1][new_level];
		new_level++;
	}
	cmd->market_level=new_level;
}

int quant_active_players(struct clients *cl_list)
{
	int active_players=0;
	while(cl_list!=NULL) {
		if(cl_list->flag_status==INGAME) {
			active_players++;
		}
		cl_list=cl_list->next;
	}
	return active_players;
}

void work_with_cmd(struct server_cmd *cmd,struct clients *cl_list)
{
	int active_players=quant_active_players(cl_list);
	cmd->month++;
	change_market_level(cmd);
	set_market_specifications(cmd,active_players);
}

struct auction {
	int price;
	int quantity;
	int player_number;
	struct auction *next;
};


void output_auction_list(struct auction *auct_list)
{
	while(auct_list!=NULL) {
		printf("==================\n");
		printf("player_number%d\n",auct_list->player_number);
		printf("%d\n",auct_list->price);
		printf("%d\n",auct_list->quantity);
		printf("==================\n");
		auct_list=auct_list->next;
	}
}
	
void fill_auct_link(struct auction *auct_tmp,int quant,int price)
{
	auct_tmp->price=price;
	auct_tmp->quantity=quant;
}

void add_link_auct_list(struct auction **auct_list,struct clients *cl_tmp,
							int mode)
{
	struct auction *auct_tmp;
	if(*auct_list==NULL) {
		*auct_list=malloc(sizeof(struct auction));
		auct_tmp=*auct_list;
	} else {
		auct_tmp=*auct_list;
		while(auct_tmp->next!=NULL) {
			auct_tmp=auct_tmp->next;
		}
		auct_tmp->next=malloc(sizeof(struct auction));
		auct_tmp=auct_tmp->next;
	}
	if(mode==BUY) {
		fill_auct_link(auct_tmp,cl_tmp->buy_mat,cl_tmp->buy_price);
	} else {
		fill_auct_link(auct_tmp,cl_tmp->sell_prod,cl_tmp->sell_price);
	}
	auct_tmp->player_number=cl_tmp->player_number;
	auct_tmp->next=NULL;
}

int return_price(struct clients *cl_list,int mode) 
{
	int price;
	if(mode==BUY) {
		price=cl_list->buy_price;
	} else {
		price=cl_list->sell_price;
	}	
	return price;
}

void create_auction_list(struct auction **auct_list,struct clients *cl_list,
							int mode)
{
	while(cl_list!=NULL) {
		int price=return_price(cl_list,mode);
		if(price!=0) {
			add_link_auct_list(auct_list,cl_list,mode);
		}
		cl_list=cl_list->next;
	}
}

struct clients *find_player(struct clients *cl_list,int player_number) 
{
	while(cl_list->player_number!=player_number) {
		cl_list=cl_list->next;
	}
	return cl_list;
}

int find_max_price(struct auction *auct_list)
{
	int max_price=0;
	int cycle=1, number_link;
	while(auct_list!=NULL) {
		if(auct_list->price>max_price) {
			number_link=cycle;
			max_price=auct_list->price;
		}
		cycle++;
		auct_list=auct_list->next;
	}
	return number_link;
}

int find_min_price(struct auction *auct_list)
{
	int min_price=7000;
	int cycle=1, number_link;
	while(auct_list!=NULL) {
		if(auct_list->price<min_price) {
			number_link=cycle;
			min_price=auct_list->price;
		}
		cycle++;
		auct_list=auct_list->next;
	}
	return number_link;
}

struct auction *correct_order(struct auction **auct_list,
				struct clients *cl_list,int mode)
{
	struct auction *auct_new_list=NULL;
	struct auction *auct_tmp;
	int number_link;
	struct auction **tmp;
	while(*auct_list!=NULL) {
		if(mode==DESCENDING) {
			number_link=find_max_price(*auct_list);
		} else {
			number_link=find_min_price(*auct_list);
		}
		tmp=&(*auct_list);
		int number=1;
		while(number!=number_link) {
			tmp=&(*tmp)->next;
			number++;
		}
		int player_number=(*tmp)->player_number;
		auct_tmp=*tmp;
		*tmp=(*tmp)->next;
		free(auct_tmp);
		struct clients *cl_tmp=cl_list;
		cl_tmp=find_player(cl_tmp,player_number);
		if(mode==DESCENDING) {
			add_link_auct_list(&auct_new_list,cl_tmp,BUY);
		} else {
			add_link_auct_list(&auct_new_list,cl_tmp,SELL);
		}
	}
	return auct_new_list;
}

int same_prices_quant_things(struct auction *auct_list) 
{
	int same_price=auct_list->price;
	int quantity=0;
	while(auct_list!=NULL) {
		if(auct_list->price==same_price) {
			quantity=quantity+auct_list->quantity;
		} else { 
			break;
		}
		auct_list=auct_list->next;
	}
	return quantity;
}

int quant_same_price(struct auction *auct_list)
{
	int same_price=auct_list->price;
	int quantity=0;
	while(auct_list!=NULL) {
		if(auct_list->price!=same_price) {
			break;
		}
		quantity++;
		auct_list=auct_list->next;
	}
	return quantity;
}

void exch_w_player_buy(struct server_cmd *cmd,struct clients *cl_list,
						int player_number)
{
	struct clients *cl_tmp=find_player(cl_list,player_number);
	int client=cl_tmp->client;
	if(cmd->quant_mat==0) {
		cl_tmp->money=
			cl_tmp->money+cl_tmp->buy_mat*cl_tmp->buy_price;
		write(client,not_succ_bauct,strlen(not_succ_bauct)+1);
		return;
	}
	if(cmd->quant_mat<cl_tmp->buy_mat) {
		while(cl_list!=NULL) {
			char buf[50];
			sprintf(buf,
				"Player #%d got %d materials for price %d\r\n",
				cl_tmp->player_number,
				cmd->quant_mat,
				cl_tmp->buy_price);
			write(cl_list->client,buf,strlen(buf)+1);
			cl_list=cl_list->next;
		}
		cl_tmp->quant_mat=cl_tmp->quant_mat+cmd->quant_mat;
		cl_tmp->money=
			cl_tmp->money+(cl_tmp->buy_mat-cmd->quant_mat)*
						cl_tmp->buy_price;
		cmd->quant_mat=0;
		return;
	} else {
		while(cl_list!=NULL) {
			char buf[50];
			sprintf(buf,
				"Player #%d got %d materials for price %d\n",
				cl_tmp->player_number,
				cl_tmp->buy_mat,
				cl_tmp->buy_price);
			write(cl_list->client,buf,strlen(buf)+1);
			cl_list=cl_list->next;
		}
		cl_tmp->quant_mat=cl_tmp->quant_mat+cl_tmp->buy_mat;
		cmd->quant_mat=cmd->quant_mat-cl_tmp->buy_mat;
	}
}

void exch_w_player_sell(struct server_cmd *cmd,struct clients *cl_list,
						int player_number)
{
	struct clients *cl_tmp=find_player(cl_list,player_number);
	int client=cl_tmp->client;
	if(cmd->quant_prod==0) {
		cl_tmp->quant_prod=cl_tmp->quant_prod+cl_tmp->sell_prod;
		write(client,not_succ_sauct,strlen(not_succ_sauct)+1);
		return;
	}
	if(cmd->quant_prod<cl_tmp->sell_prod) {
		while(cl_list!=NULL) {
			char buf[50];
			sprintf(buf,
				"Player #%d sell %d products for price %d\r\n",
				cl_tmp->player_number,
				cmd->quant_prod,
				cl_tmp->sell_price);
			write(cl_list->client,buf,strlen(buf)+1);
			cl_list=cl_list->next;
		}
		cl_tmp->quant_prod=
			cl_tmp->quant_prod+cl_tmp->sell_prod-cmd->quant_prod;
		cl_tmp->money=
			cl_tmp->money+cmd->quant_prod*cl_tmp->sell_price;
		cmd->quant_prod=0;
		return;
	} else {
		while(cl_list!=NULL) {
			char buf[50];
			sprintf(buf,
				"Player #%d sell %d products for price %d\r\n",
				cl_tmp->player_number,
				cl_tmp->sell_prod,
				cl_tmp->sell_price);
			write(cl_list->client,buf,strlen(buf)+1);
			cl_list=cl_list->next;
		}
		cl_tmp->money=cl_tmp->money+
			(cl_tmp->sell_prod*cl_tmp->sell_price);
		cmd->quant_prod=cmd->quant_prod-cl_tmp->sell_prod;
	}
}

void accept_orders(struct auction **auct_list,struct server_cmd *cmd,
					struct clients *cl_list,int mode)
{
	int same_price=(*auct_list)->price;
	struct auction *auct_tmp;
	struct auction **tmp=&(*auct_list);
	while(*tmp!=NULL) {
		if((*tmp)->price!=same_price) {
			break;
		} 
		int player_number=(*tmp)->player_number;
		if(mode==BUY) {
			exch_w_player_buy(cmd,cl_list,player_number);
		} else {
			exch_w_player_sell(cmd,cl_list,player_number);
		}
		auct_tmp=*tmp;
		*tmp=(*tmp)->next;
		free(auct_tmp);
	}
}

int get_player_number(struct auction *auct_list,int r)
{
	int i;
	for(i=1;i<r;i++) {
		auct_list=auct_list->next;
	}
	return auct_list->player_number;
}


int return_quantity(struct server_cmd cmd,int mode) 
{
	int quantity;
	if(mode==BUY) {
		quantity=cmd.quant_mat;
	} else {
		quantity=cmd.quant_prod;
	}
	return quantity;
}

void lottery(struct auction **auct_list,struct server_cmd *cmd,
					struct clients *cl_list,int mode)
{
	int quantity=return_quantity(*cmd,mode);
	while(quantity!=0) {
		int same_price=quant_same_price(*auct_list);
		int r=rand()%same_price+1;
		int player_number=get_player_number(*auct_list,r);
		struct auction **tmp=&(*auct_list);
		while(*tmp!=NULL) {
			if((*tmp)->player_number==player_number) {
				if(mode==BUY) {
					exch_w_player_buy(cmd,cl_list,
							player_number);
				} else {
					exch_w_player_sell(cmd,cl_list,
							player_number);
				}
				struct auction *auct_tmp=*tmp;
				auct_tmp=*tmp;
				*tmp=(*tmp)->next;
				free(auct_tmp);
				break;
			}
			tmp=&(*tmp)->next;
		}
		quantity=return_quantity(*cmd,mode);
	}
}

void return_all_others(struct auction **auct_list,struct server_cmd *cmd,
					struct clients *cl_list, int mode) 
{
	struct auction *auct_tmp;
	while(*auct_list!=NULL) {
		int player_number=(*auct_list)->player_number;
		if(mode==BUY) {
			exch_w_player_buy(cmd,cl_list,player_number);
		} else {
			exch_w_player_sell(cmd,cl_list,player_number);
		}
		auct_tmp=*auct_list;
		*auct_list=(*auct_list)->next;
		free(auct_tmp);
	}
}

void start_auction(struct auction **auct_list,struct server_cmd *cmd,
					struct clients *cl_list,int mode)
{
	int quantity;
	quantity=return_quantity(*cmd,mode);
	while(*auct_list!=NULL) {
		if(quantity==0) { 
			return_all_others(auct_list,cmd,cl_list,mode);
			break;
		}
		int total_quant_things=same_prices_quant_things(*auct_list);
		if(total_quant_things<=quantity) {
			accept_orders(auct_list,cmd,cl_list,mode);
		} else {
			lottery(auct_list,cmd,cl_list,mode);
		}
		quantity=return_quantity(*cmd,mode);
	}
}

void auctions_processing(struct server_cmd *cmd,struct clients *cl_list)
{
	struct auction *auct_list=NULL;
	create_auction_list(&auct_list,cl_list,BUY);
	auct_list=correct_order(&auct_list,cl_list,DESCENDING);
	if(auct_list!=NULL) {
		start_auction(&auct_list,cmd,cl_list,BUY);
	}
	create_auction_list(&auct_list,cl_list,SELL);
	auct_list=correct_order(&auct_list,cl_list,ASCENDING);
	if(auct_list!=NULL) {
		start_auction(&auct_list,cmd,cl_list,SELL);
	}
}

void check_end_of_move(struct server_cmd *cmd,struct clients *cl_list)
{
	struct clients *cl_tmp=cl_list;
	while(cl_tmp!=NULL) {
		if(cl_tmp->flag_status!=OUTGAME) {
			if(cl_tmp->flag_move!=READY) {
				return;
			}
		}
		cl_tmp=cl_tmp->next;
	}
	printf("@@@@@@@@@@@@@@@@@@END OF MOVE@@@@@@@@@@@@@@@\n");
	cl_tmp=cl_list;
	auctions_processing(cmd,cl_list);
	work_with_players(cl_list);
	work_with_cmd(cmd,cl_tmp);
	client_print(cl_list);
	printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
}

int return_client_ingame(struct clients *cl_list)
{
	int client;
	while(cl_list!=NULL) {
		if(cl_list->flag_status==INGAME) {
			client=cl_list->client;
		}
		cl_list=cl_list->next;
	}
	return client;
}

void check_end_of_game(struct server_cmd *cmd,struct clients *cl_list)
{
	int client;
	cmd->active_players=quant_active_players(cl_list);
	client=return_client_ingame(cl_list);
	if(cmd->active_players!=1) {
		return;
	}
	write(client,win,strlen(win)+1);
	/*struct clients *cl_tmp=cl_list;
	while(cl_tmp!=NULL) {
		set_cl_start_parameters(cl_tmp);
		cl_tmp=cl_tmp->next;
	}
	set_cmd_start_parameters(cmd);
	if(cmd->quant_players!=cmd->max_players) {
		cmd->flag_start=WAIT;
		return;
	}
	set_market_specifications(cmd,cmd->active_players);
	start_pl_distribution(cl_list);*/
	exit(0);
}

void cmd_print(struct server_cmd *cmd)
{
	printf("==========cmd==========\n"
		"port=%d\n"
		"max_players=%d\n"
		"quant_players=%d\n"
		"active_players=%d\n"
		"flag_start=%d\n"
		"month=%d\n"
		"market_level=%d\n"
		"quant_prod=%d\n"
		"quant_mat=%d\n"
		"min_mat_price=%d\n"
		"max_prod_price=%d\n"
		"=====================\n",
		cmd->port,
		cmd->max_players,
		cmd->quant_players,
		cmd->active_players,
		cmd->flag_start,
		cmd->month,
		cmd->market_level,
		cmd->quant_prod,
		cmd->quant_mat,
		cmd->min_mat_price,
		cmd->max_prod_price);
}


int main(int argc, char **argv)
{
	int ls;
	struct clients *cl_list=NULL;
	struct server_cmd cmd;
	set_cmd(&cmd,argv);
	ls=set_listening_socket(cmd);
	printf("Server has started successfully\n");
	for(;;) {
		int max_d;
		fd_set readfds;
		FD_ZERO(&readfds);
		readfds=fill_fd_set(cl_list,&max_d,ls);
		select(max_d+1,&readfds,NULL,NULL,NULL);
		if(FD_ISSET(ls,&readfds)) {
			new_clients(&cl_list,ls,&cmd);
		}
		work_with_clients(&cl_list,readfds,&cmd);
		if(cmd.flag_start==PROCESS) {
			check_end_of_move(&cmd,cl_list);
			check_end_of_game(&cmd,cl_list);
		}
	}
	return 0;
}
