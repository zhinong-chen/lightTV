import datetime
import socket
import threading
import tkinter as tk
import tkinter.messagebox
import psutil
from time import sleep

my_name = socket.gethostname()  # 获取本机名称
my_ip = socket.gethostbyname(my_name)  # 根据本机名称获取本机IP地址

# 创建套接字  参数（网络类型，数据流类型）
sock_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#sock_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)

sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

sock_server.bind((my_ip, 1000))  # 将socket 绑定到IP和端口上
sock_server.listen(1)  # 设置连接数量，如果连接数量大于这个值则需要排队等待

root = tk.Tk()
root.geometry("300x300")
put_esp32ip = tk.Entry(root)
computer_inf = None


def get_computer_information():
    cpu = "c:" + (str(psutil.cpu_percent(interval=1))) + '%'
    # 内存使用情况
    free = "f:" + str(round(psutil.virtual_memory().free / (1024.0 * 1024.0 * 1024.0), 2)) + "G"
    total = "t:" + str(round(psutil.virtual_memory().total / (1024.0 * 1024.0 * 1024.0), 2)) + "G"
    memory = int(psutil.virtual_memory().total - psutil.virtual_memory().free) / float(psutil.virtual_memory().total)
    # print(u"物理内存： %s " % total)
    # print(u"剩余物理内存： %s " % free)
    # print(u"物理内存使用率： %s %%" % int(memory * 100))
    now_time = datetime.datetime.fromtimestamp(psutil.boot_time()).strftime("%Y-%m-%d %H:%M:%S")
    # print(u"系统启动时间: %s" % now_time)
    memory = "m:" + str(int(memory * 100)) + "%"
    now_time = "o:" + str(now_time)
    all_information = [cpu, free, total, memory, now_time]
    return all_information


def client():
    global sock_client
    try:
        str = "%s" % put_esp32ip.get()
        # sock_client.connect(("192.168.3.7", 100))
        sock_client.connect((str, 100))
        pass
    except:
        tk.messagebox.showerror(title="IP错误", text=put_esp32ip.get())
        pass
    send_data = "c." + my_ip
    try:
        sock_client.send(send_data.encode(encoding="utf-8"))
        # print(send_data)
    except Exception as e:

        pass
    try:
        sock_client.close()
    except Exception as e:
        # print(e)
        pass
    pass


def button():
    myip_label = tk.Label(root, text=my_ip, font=('Arial', 12))
    button_1 = tk.Button(root, text='发送本机IP', font=('Arial', 12), width=10, height=1, command=client)

    button_1.pack()
    myip_label.pack()
    put_esp32ip.pack()
    pass


def server_thread():
    connection, client_address = sock_server.accept()  # 获取客户端连接服务器的状态以及IP地址和端口号
    while True:
        if connection:
            while True:
                # 设置缓冲区用于存放客户端数据
                data = connection.recv(20)  # 一次最多接受的字符
                data = data.decode("utf-8")  # 需要对接收到的数据进行格式转换
                print(data)
                sendinf(connection, data)
                pass
            pass
        else:
            connection, client_address = sock_server.accept()
            pass


def sendinf(connection, data):
    if data == "hello world!":
        connection.send("你好，世界！".encode("utf-8"))
        pass
    if data == "allinfo":
        print("ok")
        global computer_inf
        for i in range(0, 5):
            connection.send(computer_inf[i].encode(encoding="utf-8"))
            pass
        pass
    pass


def printcompinf():
    global computer_inf
    while True:
        computer_inf = get_computer_information()
        sleep(.5)  # 间隔0.5秒获取一次电脑数据
        print("....")
        pass
    pass


def thread_manager():
    t1 = threading.Thread(target=server_thread)
    t1.setDaemon(True)
    t1.start()
    t2 = threading.Thread(target=printcompinf)
    t2.setDaemon(True)
    t2.start()
    pass


if __name__ == "__main__":
    button()
    thread_manager()
    root.mainloop()
