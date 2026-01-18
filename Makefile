CC = gcc
CFLAGS = -Wall -g -I./include -I/usr/local/include/cjson
MYSQL_CFLAGS = $(shell mysql_config --cflags)
MYSQL_LIBS = $(shell mysql_config --libs)

TARGET = bin/cloud_disk
SRCDIR = src
OBJDIR = obj
LIBSDIR = libs/cJSON

# --- 2. 定义所有目标文件 ---
# 这里将 src 下的文件和 libs 下的文件统一放入 OBJS 变量
OBJS = $(OBJDIR)/conf.o \
       $(OBJDIR)/db_op.o \
       $(OBJDIR)/protocol.o \
       $(OBJDIR)/api_util.o \
       $(OBJDIR)/user_handler.o \
       $(OBJDIR)/file_handler.o \
       $(OBJDIR)/thread_pool.o \
       $(OBJDIR)/server.o \
       $(OBJDIR)/main.o \
       $(OBJDIR)/cJSON.o

all: directories $(TARGET)

#  创建目录
directories:
	@mkdir -p $(OBJDIR) bin

# --- 3. 链接规则 ---
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(MYSQL_LIBS) -lm -lpthread -lssl -lcrypto

# --- 4. 编译规则 (src 目录下的 .c) ---
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

# --- 5. 编译规则 (libs 目录下的 cJSON.c) ---
$(OBJDIR)/cJSON.o: $(LIBSDIR)/cJSON.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
