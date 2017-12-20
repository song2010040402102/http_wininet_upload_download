#include <stdio.h>
#include <windows.h>
#include <commctrl.h>
#include <Wininet.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Wininet.lib")

//上传和下载文件的缓冲区大小
#define UPLOAD_BUFFER_SIZE   1024*64
#define DOWNLOAD_BUFFER_SIZE 1024*64

//在文件上传时需要输入参数
typedef struct _UPLOAD_FILE
{
	char strIP[16]; //服务器IP地址
	char strLocalFile[MAX_PATH]; //要上传的本地文件
	char strRemoteDir[1024]; //上传到服务器的目录

}UPLOAD_FILE, *PUPLOAD_FILE;

//在文件下载时需要输入参数
typedef struct _DOWNLOAD_FILE
{
	char strIP[16]; //服务器IP地址
	char strRemoteFile[1024]; //要下载的服务器上的文件
	char strLocalFile[MAX_PATH]; //下载到的本地文件

}DOWNLOAD_FILE, *PDOWNLOAD_FILE;

char curDir[MAX_PATH] = {0}; //程序的当前目录
HWND hMainDlg = NULL; //主对话框的句柄

DOWNLOAD_FILE download_file; //文件下载结构
UPLOAD_FILE upload_file; //文件上传结构

BOOL CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam); //主对话框的回调函数
BOOL MainDlgOnInit(HWND hwndDlg, WPARAM wParam, LPARAM lParam); //执行主对话框创建时的初始化操作
BOOL MainDlgOnCommand(HWND hwndDlg, WPARAM wParam, LPARAM lParam); //主对话框中BUTTON类型控件的消息响应函数

DWORD WINAPI UploadFileProc(LPVOID lpParameter); //文件上传线程
DWORD WINAPI DownloadFileProc(LPVOID lpParameter); //文件下载线程

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	::DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDlgProc);
	return 0;
}

BOOL CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		MainDlgOnInit(hwndDlg, wParam, lParam);
		break;
	case WM_COMMAND:
		MainDlgOnCommand(hwndDlg, wParam, lParam);
		break;
	}
	return FALSE; //对于对话框处理程序，在处理完消息之后，应该返回FALSE，让系统进一步处理
}

BOOL MainDlgOnInit(HWND hwndDlg, WPARAM wParam, LPARAM lParam)
{
	hMainDlg = hwndDlg; //绑定主对话框句柄
	::GetCurrentDirectory(sizeof(curDir), curDir); //获取程序的当前目录，而且以后不会再改变

	HWND hwndIP = ::GetDlgItem(hwndDlg, IDT_IP);
	::SendMessage(hwndIP, WM_SETTEXT, 0, (LPARAM)"139.129.116.15"); //设置初始服务器的IP地址

	HWND hwndLocalFile = ::GetDlgItem(hwndDlg, IDT_UPLOAD_LOCAL_FILE);
	::SendMessage(hwndLocalFile, WM_SETTEXT, 0, (LPARAM)"C:\\upload.txt"); //设置要上传的初始本地文件

	HWND hwndRemoteDir = ::GetDlgItem(hwndDlg, IDT_UPLOAD_REMOTE_DIR);
	::SendMessage(hwndRemoteDir, WM_SETTEXT, 0, (LPARAM)"\\upload\\"); //设置要上传文件的初始远程目录

	HWND hwndRemoteFile = ::GetDlgItem(hwndDlg, IDT_DOWNLOAD_REMOTE_FILE);
	::SendMessage(hwndRemoteFile, WM_SETTEXT, 0, (LPARAM)"\\index.html"); //设置要下载文件的初始URL
	
	HWND hwndTransProgress = ::GetDlgItem(hwndDlg, IDP_TRANSFER_RATE); //进度条句柄

	::SendMessage(hwndTransProgress, PBM_SETRANGE32, 0, 100); //设置进度条的范围（0--100）
	::SendMessage(hwndTransProgress, PBM_SETPOS, 0, 0); //设置进度条的初始位置为0

	return TRUE;
}

BOOL MainDlgOnCommand(HWND hwndDlg, WPARAM wParam, LPARAM lParam)
{
	int ID = LOWORD(wParam);
	if(ID == IDCANCEL)
	{
		::EndDialog(hwndDlg, 0);
	}
	if(ID == IDB_UPLOAD_SEL_FILE) //选择要上传的本地文件
	{
		char filename[MAX_PATH] = {0};
		OPENFILENAME open_file;
		memset(&open_file, 0, sizeof(OPENFILENAME));
		open_file.lStructSize = sizeof(OPENFILENAME);
		open_file.lpstrFile = filename; //打开文件对话框中返回的文件全名
		open_file.nMaxFile = MAX_PATH;
		int id = ::GetOpenFileName(&open_file); //弹出打开文件对话框，并返回选择的文件全名
		if(id == IDOK)
		{
			::SendMessage(::GetDlgItem(hwndDlg, IDT_UPLOAD_LOCAL_FILE), WM_SETTEXT, 0, (LPARAM)filename);
		}
	}
	if(ID == IDB_UPLOAD_START) //开始上传文件
	{
		memset(&upload_file, 0, sizeof(UPLOAD_FILE));

		::SendMessage(::GetDlgItem(hwndDlg, IDT_IP), WM_GETTEXT, sizeof(upload_file.strIP), (LPARAM)upload_file.strIP);
		::SendMessage(::GetDlgItem(hwndDlg, IDT_UPLOAD_LOCAL_FILE), WM_GETTEXT, sizeof(upload_file.strLocalFile), (LPARAM)upload_file.strLocalFile);
		::SendMessage(::GetDlgItem(hwndDlg, IDT_UPLOAD_REMOTE_DIR), WM_GETTEXT, sizeof(upload_file.strRemoteDir), (LPARAM)upload_file.strRemoteDir);

		::CreateThread(NULL, 0, UploadFileProc, &upload_file, 0, 0); //启动文件上传线程
	}
	if(ID == IDB_DOWNLOAD_START) //开始下载上传
	{
		memset(&download_file, 0, sizeof(DOWNLOAD_FILE));

		::SendMessage(::GetDlgItem(hwndDlg, IDT_IP), WM_GETTEXT, sizeof(download_file.strIP), (LPARAM)download_file.strIP);
		::SendMessage(::GetDlgItem(hwndDlg, IDT_DOWNLOAD_REMOTE_FILE), WM_GETTEXT, sizeof(download_file.strRemoteFile), (LPARAM)download_file.strRemoteFile);
		strcpy(download_file.strLocalFile, curDir), strcat(download_file.strLocalFile, "\\download.txt");
		
		::CreateThread(NULL, 0, DownloadFileProc, &download_file, 0, 0); //启动文件下载线程
	}
	return TRUE;
}

DWORD WINAPI UploadFileProc(LPVOID lpParameter)
{	
	UPLOAD_FILE* upload_file = (UPLOAD_FILE*)lpParameter; //获取文件上传结构

	//初始化一个Internet会话
	HINTERNET hInternet = ::InternetOpen("HTTP_WINNET_UploadDownload", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if(!hInternet)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"InternetOpen failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		return 0;
	}

	//采用HTTP协议连接服务器
	HINTERNET hConnect = ::InternetConnect(hInternet, upload_file->strIP, 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, NULL);
	if(!hConnect)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"InternetConnect failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		::InternetCloseHandle(hInternet);
		return 0;
	}

	//建立文件上传请求
	HINTERNET hRequest = ::HttpOpenRequest(hConnect, "PUT", upload_file->strRemoteDir, "HTTP/1.1", NULL, NULL, INTERNET_FLAG_KEEP_CONNECTION, NULL);
	if(!hRequest)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpOpenRequest failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return 0;
	}	

	//打开本地已存在文件
	HANDLE hFile = ::CreateFile(upload_file->strLocalFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	int file_size = ::GetFileSize(hFile, NULL); //获取本地文件的大小

	INTERNET_BUFFERS ib;
	memset(&ib, 0, sizeof(INTERNET_BUFFERS));
	ib.dwStructSize = sizeof(INTERNET_BUFFERS);
	ib.dwBufferTotal = file_size;
	BOOL bSend = ::HttpSendRequestEx(hRequest, &ib, 0, NULL, 0); //把请求发送到服务器
	if(!bSend)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpSendRequestEx failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		::CloseHandle(hFile);
		::InternetCloseHandle(hRequest);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);	
		return 0;
	}
	
	::ShowWindow(::GetDlgItem(hMainDlg, IDP_TRANSFER_RATE), SW_SHOW);
	::EnableWindow(::GetDlgItem(hMainDlg, IDB_UPLOAD_START), FALSE);

	DWORD dwRead = 0, dwWrite = 0, sumRead = 0, sumWrite = 0, rate =0;
	char *lpBuffer = new char[UPLOAD_BUFFER_SIZE];
	do
	{
		memset(lpBuffer, 0, UPLOAD_BUFFER_SIZE);
		::ReadFile(hFile, lpBuffer, UPLOAD_BUFFER_SIZE, &dwRead, NULL); //读取本地文件的内容

		sumWrite =0;
		do
		{
			::InternetWriteFile(hRequest, lpBuffer+sumWrite, dwRead, &dwWrite); //写入到远程文件
			sumWrite += dwWrite;

		}while(sumWrite< dwRead);

		sumRead += dwRead;
		
		rate = (double)sumRead*100/file_size; //这里需要临时转化成double，否则对于大文件会越界
		::SendMessage(::GetDlgItem(hMainDlg, IDP_TRANSFER_RATE), PBM_SETPOS, rate, 0);	//设置当前传输的进度

	}while(sumRead<file_size);

	delete []lpBuffer;

	::ShowWindow(::GetDlgItem(hMainDlg, IDP_TRANSFER_RATE), SW_HIDE);
	::EnableWindow(::GetDlgItem(hMainDlg, IDB_UPLOAD_START), TRUE);

	::CloseHandle(hFile);
	::InternetCloseHandle(hRequest);
	::InternetCloseHandle(hConnect);
	::InternetCloseHandle(hInternet);	

	MessageBox(hMainDlg, "文件上传成功", "success", MB_OK);
	return 0;
}

DWORD WINAPI DownloadFileProc(LPVOID lpParameter)
{
	DOWNLOAD_FILE* download_file = (DOWNLOAD_FILE*)lpParameter; //获取文件下载结构

	//初始化一个Internet会话
	HINTERNET hInternet = ::InternetOpen("HTTP_WINNET_UploadDownload", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if(!hInternet)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"InternetOpen failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		return 0;
	}

	//获取超时时间
	DWORD dwTime = 0, dwLen = 0;
	::InternetQueryOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTime, &dwLen);
	dwTime = 0;
	::InternetQueryOption(hInternet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &dwTime, &dwLen);
	dwTime = 0;
	::InternetQueryOption(hInternet, INTERNET_OPTION_DATA_SEND_TIMEOUT, &dwTime, &dwLen);
	dwTime = 0;
	::InternetQueryOption(hInternet, INTERNET_OPTION_DISCONNECTED_TIMEOUT, &dwTime, &dwLen);
	dwTime = 0;
	::InternetQueryOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTime, &dwLen);
	dwTime = 0;
	::InternetQueryOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &dwTime, &dwLen);

	//设置超时时间
	dwTime = 10*1000;
	::InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTime, 4);
	dwTime = 10*1000;
	::InternetSetOption(hInternet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &dwTime, 4);
	dwTime = 10*1000;
	::InternetSetOption(hInternet, INTERNET_OPTION_DATA_SEND_TIMEOUT, &dwTime, 4);
	dwTime = 10*1000;
	::InternetSetOption(hInternet, INTERNET_OPTION_DISCONNECTED_TIMEOUT, &dwTime, 4);
	dwTime = 10*1000;
	::InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTime, 4);
	dwTime = 10*1000;
	::InternetSetOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &dwTime, 4);	

	//采用HTTP协议连接服务器
	HINTERNET hConnect = ::InternetConnect(hInternet, download_file->strIP, 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, NULL);
	if(!hConnect)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"InternetConnect failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		::InternetCloseHandle(hInternet);
		return 0;
	}

	//建立文件头请求，目的是要获取下载文件的大小
	HINTERNET hRequest = ::HttpOpenRequest(hConnect, "HEAD", download_file->strRemoteFile, "HTTP/1.1", NULL, NULL, INTERNET_FLAG_KEEP_CONNECTION, NULL);
	if(!hRequest)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpOpenRequest failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return 0;
	}

	BOOL bSend = ::HttpSendRequest(hRequest, NULL, 0, NULL, 0); //把请求发送到服务器
	if(!bSend)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpSendRequest failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);		
		::InternetCloseHandle(hRequest);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);	
		return 0;
	}
	//查询刚才发送请求所对应的响应的信息
	DWORD dwContentLen = 0, dwSize = sizeof(dwContentLen);
	BOOL bQuery = HttpQueryInfo(hRequest, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH, &dwContentLen, &dwSize, NULL);  
	if(!bQuery)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpQueryInfo failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);		
		::InternetCloseHandle(hRequest);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return 0;
	}
	::InternetCloseHandle(hRequest);

	//建立文件下载请求
	hRequest = ::HttpOpenRequest(hConnect, "GET", download_file->strRemoteFile, "HTTP/1.1", NULL, NULL, INTERNET_FLAG_KEEP_CONNECTION, NULL);
	if(!hRequest)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpOpenRequest failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return 0;
	}

	bSend = ::HttpSendRequest(hRequest, NULL, 0, NULL, 0); //把请求发送到服务器
	if(!bSend)
	{
		char errorInfo[128] = {0};
		sprintf(errorInfo,"HttpSendRequest failed with code %d.", GetLastError());
		MessageBox(hMainDlg, errorInfo, "fail", MB_OK | MB_ICONERROR);		
		::InternetCloseHandle(hRequest);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);	
		return 0;
	}
	
	//打开本地已存在文件
	HANDLE hFile = ::CreateFile(download_file->strLocalFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	
	::ShowWindow(::GetDlgItem(hMainDlg, IDP_TRANSFER_RATE), SW_SHOW);
	::EnableWindow(::GetDlgItem(hMainDlg, IDB_DOWNLOAD_START), FALSE);

	DWORD dwRead = 0, dwWrite = 0, sumRead = 0, sumWrite = 0, rate = 0;	
	char *lpBuffer = new char[DOWNLOAD_BUFFER_SIZE];
	FILE * file = NULL;
	do
	{		
		memset(lpBuffer, 0, DOWNLOAD_BUFFER_SIZE);
		::InternetReadFile(hRequest, lpBuffer, DOWNLOAD_BUFFER_SIZE, &dwRead); //读取远程文件	

		sumWrite = 0;
		do
		{
			::WriteFile(hFile, lpBuffer+sumWrite, dwRead, &dwWrite, NULL); //写入到本地文件
			sumWrite += dwWrite;

		}while(sumWrite<dwRead);

		sumRead += dwRead;
		
		rate = (double)sumRead*100/dwContentLen; //这里需要临时转化成double，否则对于大文件会越界
		::SendMessage(::GetDlgItem(hMainDlg, IDP_TRANSFER_RATE), PBM_SETPOS, rate, 0);	//设置当前传输的进度		

	}while(sumRead<dwContentLen);

	delete []lpBuffer;

	::ShowWindow(::GetDlgItem(hMainDlg, IDP_TRANSFER_RATE), SW_HIDE);
	::EnableWindow(::GetDlgItem(hMainDlg, IDB_DOWNLOAD_START), TRUE);

	::CloseHandle(hFile);
	::InternetCloseHandle(hRequest);
	::InternetCloseHandle(hConnect);
	::InternetCloseHandle(hInternet);	

	MessageBox(hMainDlg, "文件下载成功", "success", MB_OK);
	return 0;
}