#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>


//功率值到坐标值的转换
#define TOY(val)	(-1*val)
//		y = (cfg.yref+buffer[i-1]);//dBm值
//		y *= -1*50/cfg.ydiv;//坐标值
#define power2axis(val)	((cfg.yref + val) * -1 * 50 /cfg.ydiv)
struct {
	float yref;
	float ydiv;
	double fs;
	double flo;
	gboolean freq_mirror;
	gboolean high_lo;
}cfg={
	0,
	10,
	179.2,
	363.4,
	0,
	0,
};
float buffer[1024]={0};//帧数据缓冲区
float yscale=1;


int autorun=1;//更新/停止？
int maxhold=0;//最大值保持/实时更新？

unsigned int active_mkr;//当前活动Marker索引
unsigned int x_marker[10]={0};
unsigned int update_marker = 1;
GtkWidget *canvas;
GtkBuilder *builder;

gpointer refresh(gpointer data);

void fft(float xreal [], float ximag [], int n);
int calc_pow(short *buf, unsigned int len);
G_MODULE_EXPORT
int paint (GtkWidget *widget, GdkEvent *event, gpointer data);

G_MODULE_EXPORT
void update_fslo(GtkButton *btn, gpointer data)
{
	GtkEntry *entry;
	float val;

	entry = GTK_ENTRY(gtk_builder_get_object(builder, "entry_fs"));
	cfg.fs = (float)g_strtod(entry->text,NULL);

	entry = GTK_ENTRY(gtk_builder_get_object(builder, "entry_flo"));
	cfg.flo = g_strtod(entry->text,NULL);

	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "dlg_fslo_setting")));
}

//Marker表编辑框更新内容回调函数
G_MODULE_EXPORT
int mark_table_edit_start(GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, gpointer user_data)
{
	update_marker = 0;
}

G_MODULE_EXPORT
int mark_table_edit_finished(GtkCellRendererText *rd, gchar *path, gchar *text, gpointer p)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	int index=0;
	float value=0;
	sscanf(path, "%d", &index);
	sscanf(text, "%f", &value);

	model = GTK_TREE_MODEL(gtk_builder_get_object(builder, "liststore_marker"));
	gtk_tree_model_iter_nth_child(model, &iter, NULL, index);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 4, value, -1);

	//折算X坐标
	value = (value+cfg.fs-abs(cfg.flo)) *1024/cfg.fs;
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, value, -1);

	update_marker = 1;
}

G_MODULE_EXPORT
void high_lo_sw(GtkToggleButton *tg, gpointer user_data)
{
	cfg.high_lo = gtk_toggle_button_get_active(tg);
}

G_MODULE_EXPORT
void freq_mirror_sw(GtkToggleButton *tg, gpointer user_data)
{
	cfg.freq_mirror = gtk_toggle_button_get_active(tg);
}

G_MODULE_EXPORT
void marker_visable_switch(GtkCellRendererToggle *rd, gchar *path, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean visable=0;
	int index=0;

	sscanf(path, "%d", &index);
	visable = gtk_cell_renderer_toggle_get_activatable(rd);

	model = GTK_TREE_MODEL(gtk_builder_get_object(builder, "liststore_marker"));
	gtk_tree_model_iter_nth_child(model, &iter, NULL, index);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 2, visable, -1);
}
G_MODULE_EXPORT
void current_marker_switch(GtkCellRendererToggle *renderer, gchar *path, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	int total_cnt, i;

	sscanf(path, "%d", &active_mkr);

	//先将所有的不激活
	model = GTK_TREE_MODEL(gtk_builder_get_object(builder, "liststore_marker"));
	total_cnt = gtk_tree_model_iter_n_children(model, NULL);
	for (i=0; i<total_cnt; i++) {
		gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, 3, 0, -1);
	}

	//然后只激活指定的
	gtk_tree_model_iter_nth_child(model, &iter, NULL, active_mkr);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 3, 1, -1);

}


void update_axis(void)
{
	char str[50];
	int i;
	GtkWidget *label;

	for (i=1; i<11; i++) {
		sprintf(str, "label_y%d", i);
		label = GTK_WIDGET(gtk_builder_get_object(builder, str));
		sprintf(str, "%-3.2f", i*cfg.ydiv+cfg.yref);
		gtk_label_set_text(GTK_LABEL(label), str);
	}
}
/*
 *	坐标轴伸缩
 *
 *
 */
G_MODULE_EXPORT
void adj_yref_cb(GtkAdjustment *adj, gpointer data)
{
	cfg.yref = gtk_adjustment_get_value(adj);
	update_axis();
	gtk_widget_queue_draw(canvas);
}
G_MODULE_EXPORT
void adj_ydiv_cb(GtkAdjustment *adj, gpointer data)
{
	cfg.ydiv = gtk_adjustment_get_value(adj);
	update_axis();
	gtk_widget_queue_draw(canvas);
}
G_MODULE_EXPORT
void btn_maxhold_cb(GtkButton *btn, gpointer data)
{
	maxhold = !maxhold;
	if (maxhold) {
		gtk_button_set_label(btn, "实时刷新");
	} else {
		gtk_button_set_label(btn, "最大保持");
	}
}

/***
 *	自动运行切换按钮回调函数
 */
G_MODULE_EXPORT
void btn_autorun_cb(GtkButton *btn, gpointer data)
{
	autorun = !autorun;
	if (autorun) {
		gtk_button_set_label(btn, "停止更新");
	} else {
		gtk_button_set_label(btn, "自动运行");
	}
}

/**
 *	绘图区鼠标按键回调函数（更新Marker的频率）
 */
G_MODULE_EXPORT
gboolean canvas_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer *data)
{
	int index;
	float freq,power;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkComboBox *combox;

	//获取iter
	model = GTK_TREE_MODEL(gtk_builder_get_object(builder, "liststore_marker"));
	gtk_tree_model_iter_nth_child(model, &iter, NULL, active_mkr);

	index = event->x;
	power = buffer[index];

	printf("flo=%f fs=%f freq=%f %d %d\n", cfg.flo, cfg.fs, freq, cfg.freq_mirror, cfg.high_lo);

	//更新
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0,event->x, -1);//X坐标
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1,power, -1);//功率

	//TODO
	//	按照高低本振、频谱镜像
	//if (cfg.high_lo) {
	//} else {
	//}
	//gtk_list_store_set(GTK_LIST_STORE(model), &iter, 4,freq, -1);//频率

	gtk_widget_queue_draw(canvas);
	return TRUE;
}

int main (gint argc, gchar **argv)
{
#ifdef WIN32
	WSADATA Ws;
	WSAStartup(MAKEWORD(2,2), &Ws);
#endif
	if(!g_thread_supported())
		g_thread_init(NULL);
	gdk_threads_init();
	gtk_init (&argc, &argv);

	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, "adcap.glade", NULL);
	gtk_builder_connect_signals(builder, NULL);

	canvas = GTK_WIDGET(gtk_builder_get_object(builder, "drawing_area"));
	g_signal_connect(canvas, "expose-event", G_CALLBACK(paint), NULL);
	gtk_builder_connect_signals(builder, NULL);


	//创建新线程，用于监听并刷新控件
	g_thread_create(refresh, NULL, 0, NULL);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}

void paint_bg(cairo_t *cr)
{
	unsigned int i;
	char str[50];

	cairo_set_line_width(cr, 4);
	cairo_set_source_rgb(cr, 0, 0, 0);

	//外框
	cairo_move_to(cr,   1,   1);
	cairo_line_to(cr,   1, 499);
	cairo_line_to(cr, 511, 499);
	cairo_line_to(cr, 511,   1);
	cairo_line_to(cr,   1,   1);
	cairo_stroke(cr);
	//网格
	cairo_set_line_width(cr, 1);
	cairo_set_source_rgb(cr, 5, 5, 1);
	for (i=1; i<512; i+=51) {
		cairo_move_to(cr, i, 0);
		cairo_line_to(cr, i, 500);
	}
	for (i=1; i<500; i+=50) {
		cairo_move_to(cr, 0, i);
		cairo_line_to(cr, 512, i);
	}
	cairo_stroke(cr);
}

void paint_marker(cairo_t *cr)
{
	float x;
	unsigned int index=0;
	gboolean visable;
	GtkTreeModel *model;
	GtkTreeIter iter;
	int total_cnt, i;
	double y;


	model = GTK_TREE_MODEL(gtk_builder_get_object(builder, "liststore_marker"));
	total_cnt = gtk_tree_model_iter_n_children(model, NULL);


	//画固定Marker点
	cairo_set_line_width(cr, 1);
	cairo_set_source_rgb(cr, 0, 1, 0);

	for (i=0; i<total_cnt; i++) {
		gtk_tree_model_iter_nth_child(model, &iter, NULL, i);

		gtk_tree_model_get(model, &iter, 2,&visable, -1);
		gtk_tree_model_get(model, &iter, 0,&x, -1);
		if (update_marker) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1,buffer[(unsigned int)x], -1);
		}

		if (!visable) {
			continue;
		}

		y = power2axis(buffer[(unsigned int)x]);
		if (y>500 || x>512) {
			continue;
		}

		cairo_new_path(cr);
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x-3, y-8);
		cairo_line_to(cr, x, y-12);
		cairo_line_to(cr, x+3, y-8);
		cairo_line_to(cr, x, y);
		cairo_fill(cr);
	}
	cairo_stroke(cr);
}

/**
	 根据共享数组，刷新显示
  */
G_MODULE_EXPORT
int paint(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	cairo_t *cr;
	int i;
	double y;

	cr = gdk_cairo_create (widget->window);

	paint_bg(cr);
	paint_marker(cr);

	//画图（蓝色）
	cairo_set_line_width(cr, 1);
	cairo_set_source_rgb(cr, 0, 0, 1);
	y = TOY(buffer[0])>500 ? 500 : TOY(buffer[0]);
	cairo_move_to(cr, 0, TOY(buffer[0]));
	for (i=1; i<512; i++) {
		//y轴分为10个div，每个div包含50个点
		//因此每1dBm对应的y坐标应该为50/ydiv
		//y = (cfg.yref+buffer[i-1]);//dBm值
		//y *= -1*50/cfg.ydiv;//坐标值
		y = power2axis(buffer[i-1]);
		cairo_line_to(cr, i-1, y>500?500:y);
	}
	cairo_stroke(cr);

	cairo_destroy(cr);
	return TRUE;
}

/*
 	创建套接字并触发界面刷新
 
 */

gpointer refresh(gpointer data)
{
	short sock_buff[1050];
	int sockfd,ret;
	struct sockaddr_in server_addr;


	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd<0) {
		perror("Fail to create socket");
		return  NULL;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(10000);

	bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	while (1) {
#ifdef WIN32
		Sleep(10);
		if (!autorun) {
			Sleep(10);
			continue;
		}
#else
		usleep(1);
		if (!autorun) {
			sleep(1);
			continue;
		}
#endif

		ret = recvfrom(sockfd, (char *)sock_buff, sizeof(sock_buff), 0, NULL, NULL);
		if (ret>0) {
			calc_pow(sock_buff, 1024);
			gdk_threads_enter();
			gtk_widget_queue_draw(canvas);
			gdk_threads_leave();
		}
	}
#ifdef WIN32
	closesocket(sockfd);
#else
	close(sockfd);
#endif

}

/*
 * 	计算功率
 * 单位：dBm
 * 
 * 说明：
 * 	将dat中的数据fft后计算功率（单位：dBFs）
 */
int calc_pow(short *buff, unsigned int len)
{
	float *di,*dr; //di=>虚部，dr=>实部
	float max,min;
	int i;

	dr = malloc(sizeof(float)*len);
	di = malloc(sizeof(float)*len);

	memset(di, 0, sizeof(float)*len);
	for (i=0; i<1024; i++) {
		//归一化、使得幅值在0~1之间
		dr[i] = 1.0*buff[i]/0x7FFF;
		//hanming窗
		dr[i] *= 0.54-0.46*cos(2*3.141598*(i+1)/1023);
	}

	fft(dr, di, len);

	//计算对数功率
	max = -90000;
	for (i=0; i<len; i++) {
		dr[i]  = sqrt(dr[i]*dr[i] + di[i]*di[i])/1024;
		//FFT的功率谱分布在完全相同的原谱和镜像两部分
		//所以总功率分布在这两部分，需要叠加
		dr[i] *= 2;
		dr[i] = 10*log10(dr[i]);
	}

	if (maxhold) {
		for (i=0; i<len; i++) {
			buffer[i] = buffer[i]>dr[i] ? buffer[i] : dr[i];
		}
	} else {
		for (i=0; i<len; i++) {
			buffer[i] = dr[i];
		}
	}

	free(di);
	free(dr);

	return 0;
}


G_MODULE_EXPORT
void btn_save_cb(GtkButton *btn, gpointer data)
{
	cairo_t *cr;
	cairo_surface_t *surface;
	int i;
	double y;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 512, 500);
	cr = cairo_create (surface);

	paint_bg(cr);
	paint_marker(cr);

	//画图（蓝色）
	cairo_set_line_width(cr, 1);
	cairo_set_source_rgb(cr, 0, 0, 1);
	y = TOY(buffer[0])>500 ? 500 : TOY(buffer[0]);
	cairo_move_to(cr, 0, TOY(buffer[0]));
	for (i=1; i<512; i++) {
		y = cfg.yref + yscale*TOY(buffer[i-1]);
		//cairo_line_to(cr, i-1, cfg.yref+yscale*TOY(buffer[i-1]));
		cairo_line_to(cr, i-1, y>500?500:y);
	}
	cairo_stroke(cr);

	cairo_surface_write_to_png(surface, "save.png");

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

