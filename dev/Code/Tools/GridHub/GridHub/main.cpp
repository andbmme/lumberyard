/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

// windows include must be first so we get the full version (AZCore bring the trimmed one)
#define NOMINMAX
#ifdef _WIN32
#include <Windows.h>
#include <winnls.h>
#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#endif

#include "gridhub.hxx"
AZ_PUSH_DISABLE_WARNING(4244 4251, "-Wunknown-warning-option")
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QSharedMemory>
#include <QtCore/QProcess>
AZ_POP_DISABLE_WARNING

#include <AzCore/EBus/EBus.h>
#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Memory/MemoryComponent.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/parallel/mutex.h>

#ifdef AZ_PLATFORM_WINDOWS
#include <Shlwapi.h>
#elif defined AZ_PLATFORM_APPLE
#include <mach-o/dyld.h>
#include <errno.h>
#include <sys/param.h>
#include <libgen.h>
#else
#include <sys/param.h>
#include <errno.h>
#endif

#ifdef AZ_PLATFORM_WINDOWS
#define GRIDHUB_TSR_SUFFIX _T("_copyapp_")
#define GRIDHUB_TSR_NAME _T("GridHub_copyapp_.exe")
#define GRIDHUB_IMAGE_NAME _T("GridHub.exe")
#else
#define GRIDHUB_TSR_SUFFIX "_copyapp_"
#define GRIDHUB_TSR_NAME "GridHub_copyapp_"
#define GRIDHUB_IMAGE_NAME "GridHub"
#endif

#if AZ_TRAIT_OS_PLATFORM_APPLE
#ifdef _DEBUG
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

static bool IsDebuggerPresent()
    // Returns true if the current process is being debugged (either
    // running under the debugger or has a debugger attached post facto).
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.

    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}
#else
static bool IsDebuggerPresent()
{
    return false;
}
#endif
#elif defined AZ_PLATFORM_LINUX
static bool IsDebuggerPresent()
{
    //KDAB_TODO
    return false;
}
#endif

/**
 * Editor Application \ref ComponentApplication.
 */
class GridHubApplication : public AZ::ComponentApplication, AZ::SystemTickBus::Handler
{
public:
    GridHubApplication() : m_monitorForExeChanges(false), m_needToRelaunch(false), m_timeSinceLastCheckForChanges(0.0f) {}
    /// Create application, if systemEntityFileName is NULL, we will create with default settings.
    AZ::Entity* Create(const char* systemEntityFileName, const StartupParameters& startupParameters = StartupParameters()) override;
    void Destroy() override;

    bool            IsNeedToRelaunch() const        { return m_needToRelaunch; }
    bool            IsValidModuleName() const       { return m_monitorForExeChanges; }
    const QString   GetModuleName() const           { return m_originalExeFileName; }
protected:
    /**
     * This is the function that will be called instantly after the memory
     * manager is created. This is where we should register all core component
     * factories that will participate in the loading of the bootstrap file
     * or all factories in general.
     * When you create your own application this is where you should FIRST call
     * ComponentApplication::RegisterCoreComponents and then register the application
     * specific core components.
     */
     virtual void RegisterCoreComponents();

     /**
          AZ::SystemTickBus::Handler
     */
     void OnSystemTick() override
     {
         // Calculate deltaTime
         AZStd::chrono::system_clock::time_point now = AZStd::chrono::system_clock::now();
         static AZStd::chrono::system_clock::time_point lastUpdate = now;

         AZStd::chrono::duration<float> delta = now - lastUpdate;
         float deltaTime = delta.count();

         lastUpdate = now;

        /// This is called from a 'safe' main sync point and should originate all messages that need to be synced to the 'main' thread.
        // {
        //  if (AZ::IO::Streamer::IsReady())
        //  AZ::IO::Streamer::Instance().ReceiveRequests(); // activate callbacks on main thread!
        // }

         // check to see if we got a newer version of our executable, if so run it.
         if( m_monitorForExeChanges )
         {
             m_timeSinceLastCheckForChanges += deltaTime;
             if( m_timeSinceLastCheckForChanges > 5.0f ) // check every 5 seconds
             {
                 m_timeSinceLastCheckForChanges -= 5.0f;
                 const QDateTime fileLastModified = QFileInfo(m_originalExeFileName).lastModified();
                 if (!fileLastModified.isNull())
                 {
                     if (fileLastModified != m_originalExeLastModified)
                     {
                         AZ_Printf("GridHub","Detected exe file change quitting...");
                         // we need to quit the APP and do copy and run
                         m_needToRelaunch = true;
                         qApp->quit();
                     }
                 }
             }
         }
     }

    QString         m_originalExeFileName;
     QDateTime      m_originalExeLastModified;
     bool           m_monitorForExeChanges;
     bool           m_needToRelaunch;
     float          m_timeSinceLastCheckForChanges;
};

/**
 * Gridhub application.
 */
class QGridHubApplication : public QApplication, public QAbstractNativeEventFilter
{
public:
    QGridHubApplication(int &argc, char **argv) : QApplication(argc,argv)
    {
#ifdef AZ_PLATFORM_WINDOWS
        CoInitialize(NULL);
#endif
        m_systemEntity = NULL;
        m_gridHubComponent = NULL;
        m_systemEntityFileName = "GridHubConfig.xml";
        installNativeEventFilter(this);
    }

    ~QGridHubApplication()
    {
        removeNativeEventFilter(this);
    }

    //virtual bool QGridHubApplication::winEventFilter( MSG *msg , long *result)
    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result)
    {
#ifdef AZ_PLATFORM_WINDOWS
        if ((eventType == "windows_generic_MSG")||(eventType == "windows_dispatcher_MSG"))
        {
            MSG* msg = reinterpret_cast<MSG*>(message);
            switch (msg->message) {
                case WM_QUERYENDSESSION:
                    Finalize();
                    break;
            }
        }
#endif
        return false;
    }

    void Initialize()
    {
        m_systemEntity = m_componentApp.Create(m_systemEntityFileName);
        AZ_Assert(m_systemEntity, "Unable to create the system entity!");

        if( !m_systemEntity->FindComponent<AZ::MemoryComponent>() )
        {
            m_systemEntity->CreateComponent<AZ::MemoryComponent>();
        }

        if( !m_systemEntity->FindComponent<GridHubComponent>() )
            m_systemEntity->CreateComponent<GridHubComponent>();

        m_gridHubComponent = m_systemEntity->FindComponent<GridHubComponent>();

        if (m_componentApp.IsValidModuleName())
            AddToStartupFolder(m_componentApp.GetModuleName(),m_gridHubComponent->IsAddToStartupFolder());

        m_systemEntity->Init();
        m_systemEntity->Activate();

        if (styleSheet().isEmpty())
        {
            QDir::addSearchPath("UI", ":/GridHub/Resources/StyleSheetImages");
            QFile file(":/GridHub/Resources/style_dark.qss");
            file.open(QFile::ReadOnly);
            QString styleSheet = QLatin1String(file.readAll());
            setStyleSheet(styleSheet);
        }
    }

    int Execute()
    {
        GridHub mainWnd(&m_componentApp,m_gridHubComponent);
        m_gridHubComponent->SetUI(&mainWnd);
        // show the window only when we debug
        if( !m_componentApp.IsValidModuleName() )
            mainWnd.show();
        return exec();
    }

    void Finalize()
    {
        if( m_systemEntity )
        {
            m_systemEntity->Deactivate();

            // Write the current state of the system components into cfg file.
            m_componentApp.WriteApplicationDescriptor(m_systemEntityFileName);

            if (m_componentApp.IsValidModuleName())
                AddToStartupFolder(m_componentApp.GetModuleName(),m_gridHubComponent->IsAddToStartupFolder());

            m_componentApp.Destroy();
            m_systemEntity = NULL;
            m_gridHubComponent = NULL;
        }
    }

    bool IsNeedToRelaunch()
    {
        if (m_systemEntity)
        {
            return m_componentApp.IsNeedToRelaunch();
        }
        return false;
    }

    static void AddToStartupFolder(const QString& moduleFilename, bool isAdd)
    {
#ifdef AZ_PLATFORM_WINDOWS
        HRESULT hres;
        TCHAR startupFolder[MAX_PATH] = {0};
        TCHAR fullLinkName[MAX_PATH] = {0};

        LPITEMIDLIST pidlFolder = NULL;
        hres = SHGetFolderLocation(0,/*CSIDL_COMMON_STARTUP all users required admin access*/CSIDL_STARTUP,NULL,0,&pidlFolder);
        if (SUCCEEDED(hres))
        {
            if( SHGetPathFromIDList(pidlFolder,startupFolder) )
            {
                _tcscat_s(fullLinkName,startupFolder);
                _tcscat_s(fullLinkName,"\\Amazon Grid Hub.lnk");
            }
            CoTaskMemFree(pidlFolder);
        }

        if( moduleFilename.isEmpty() || _tcslen(fullLinkName) == 0 )
            return;

        // for development, never autoadd to startup
        if( moduleFilename.contains("gridmate\\development", Qt::CaseInsensitive))
            isAdd = false;

        if( isAdd )
        {
            // add to start up folder

            // get my file full name
            IShellLink* psl;

            // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
            // has already been called.
            hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
            if (SUCCEEDED(hres))
            {
                IPersistFile* ppf;

                // Set the path to the shortcut target and add the description.
                psl->SetPath(moduleFilename.toUtf8().data());
                psl->SetDescription("Amazon Grid Hub");

                // Query IShellLink for the IPersistFile interface, used for saving the
                // shortcut in persistent storage.
                hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

                if (SUCCEEDED(hres))
                {
                    WCHAR wsz[MAX_PATH];

                    // Ensure that the string is Unicode.
                    MultiByteToWideChar(CP_ACP, 0, fullLinkName, -1, wsz, MAX_PATH);

                    // Add code here to check return value from MultiByteWideChar
                    // for success.

                    // Save the link by calling IPersistFile::Save.
                    hres = ppf->Save(wsz, TRUE);
                    ppf->Release();
                }
                psl->Release();
            }
        }
        else
        {
            // remove from start up folder
            DeleteFile(fullLinkName);
        }
#endif
#if AZ_TRAIT_OS_PLATFORM_APPLE
        if (isAdd)
        {
            const QString command = "tell application \"System Events\" to make login item at end with properties {path:\"%1\"}";
            QString path = moduleFilename;
            QDir dir(QFileInfo(path).absoluteDir());
            dir.cdUp();
            dir.cdUp();
            if (dir.dirName() == GRIDHUB_IMAGE_NAME".app")
            {
                path = dir.path();
            }
            QProcess::startDetached("/usr/bin/osascript", {"-e", command.arg(path)});
        }
        else
        {
            const QString command = "tell application \"System Events\" to delete login item \"%1\"";
            QProcess::startDetached("/usr/bin/osascript", {"-e", command.arg(GRIDHUB_IMAGE_NAME)});
        }
#endif
    }

    const char* m_systemEntityFileName;
    AZ::Entity* m_systemEntity;
    GridHubComponent* m_gridHubComponent;
    GridHubApplication m_componentApp;
};


//=========================================================================
// Create
// [6/18/2012]
//=========================================================================
AZ::Entity*
GridHubApplication::Create(const char* systemEntityFileName, const StartupParameters& startupParameters /* = StartupParameters() */)
{
    m_monitorForExeChanges = IsDebuggerPresent() ? false : true;
    if( m_monitorForExeChanges )
    {
        bool isError = false;
#ifdef AZ_PLATFORM_WINDOWS
        TCHAR originalExeFileName[MAX_PATH];
        if( GetModuleFileName(NULL,originalExeFileName,AZ_ARRAY_SIZE(originalExeFileName)) )
        {
            PathRemoveFileSpec(originalExeFileName);
            PathAppend(originalExeFileName, GRIDHUB_IMAGE_NAME);

            m_originalExeFileName = originalExeFileName;

            m_originalExeLastModified = QFileInfo(m_originalExeFileName).lastModified();
        }
#elif defined AZ_PLATFORM_LINUX
        // KDAB_TODO
        if ( 0 ) //Avoid compile error
        {
        }
#else
        char path[MAXPATHLEN];
        unsigned int pathSize = MAXPATHLEN;
        if (_NSGetExecutablePath(path, &pathSize) == 0)
        {
            m_originalExeFileName = QDir(dirname(path)).absoluteFilePath(GRIDHUB_IMAGE_NAME);
            m_originalExeLastModified = QFileInfo(m_originalExeFileName).lastModified();
        }
#endif
        else
        {
            isError = true;
        }

        if( isError )
        {
#ifdef AZ_PLATFORM_WINDOWS
            AZ_Printf("GridHub","Failed to get module file name %d\n",GetLastError());
#else
            AZ_Printf("GridHub","Failed to get module file name %d\n",strerror(errno));
#endif
            m_monitorForExeChanges = false;
        }
    }

    // note: since we accessing directly 'systemEntityFileName' should be the full file name
    if( systemEntityFileName && AZ::IO::SystemFile::Exists(systemEntityFileName) )
    {
        AZ::Entity* sysEntity = ComponentApplication::Create(systemEntityFileName, startupParameters);
        if (sysEntity)
            AZ::SystemTickBus::Handler::BusConnect();
        return sysEntity;
    }
    else
    {
        ComponentApplication::Descriptor appDesc;
        AZ::Entity* sysEntity = ComponentApplication::Create(appDesc, startupParameters);
        if (sysEntity)
            AZ::SystemTickBus::Handler::BusConnect();
        return sysEntity;
    }
}

//=========================================================================
// Destroy
// [7/6/2012]
//=========================================================================
void GridHubApplication::Destroy()
{
    AZ::SystemTickBus::Handler::BusDisconnect();
    AZ::ComponentApplication::Destroy();
}

//=========================================================================
// RegisterCoreComponents
// [6/18/2012]
//=========================================================================
void GridHubApplication::RegisterCoreComponents()
{
    ComponentApplication::RegisterCoreComponents();

    // GridHub components
    GridHubComponent::CreateDescriptor();
}

//=========================================================================
// CopyAndRun
// [10/24/2012]
//=========================================================================
void CopyAndRun(bool failSilently)
{
#ifdef AZ_PLATFORM_WINDOWS
    TCHAR myFileName[MAX_PATH] = { _T(0) };
    if (GetModuleFileName(NULL, myFileName, MAX_PATH ))
    {
        TCHAR sourceProcPath[MAX_PATH] = { _T(0) };
        TCHAR targetProcPath[MAX_PATH] = { _T(0) };
        TCHAR procDrive[MAX_PATH] = { _T(0) };
        TCHAR procDir[MAX_PATH] = { _T(0) };
        TCHAR procFname[MAX_PATH] = { _T(0) };
        TCHAR procExt[MAX_PATH] = { _T(0) };
        _tsplitpath_s(myFileName, procDrive, procDir, procFname, procExt);
        _tmakepath_s(sourceProcPath, procDrive, procDir, GRIDHUB_IMAGE_NAME, NULL);
        _tmakepath_s(targetProcPath, procDrive, procDir, GRIDHUB_TSR_NAME, NULL);
        if (CopyFileEx(sourceProcPath, targetProcPath, NULL, NULL, NULL, 0))
        {
            STARTUPINFO si;
            PROCESS_INFORMATION pi;

            memset(&si, 0, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;

            CreateProcess(
                NULL,                       // No module name (use command line).
                targetProcPath,             // Command line.
                NULL,                       // Process handle not inheritable.
                NULL,                       // Thread handle not inheritable.
                FALSE,                      // Set handle inheritance to FALSE.
                CREATE_NEW_PROCESS_GROUP,   // Don't use the same process group as the current process!
                NULL,                       // Use parent's environment block.
                NULL,                       // Startup in the same directory as the executable
                &si,                        // Pointer to STARTUPINFO structure.
                &pi);                       // Pointer to PROCESS_INFORMATION structure.
        }
        else
        {
            if (!failSilently)
            {
                TCHAR errorMsg[1024] = { _T(0) };
                _stprintf_s(errorMsg, _T("Failed to copy GridHub. Make sure that %s%s is writable!"), procFname, procExt);
                MessageBox(NULL, errorMsg, NULL, MB_ICONSTOP|MB_OK);
            }
        }
    }
#elif defined AZ_PLATFORM_LINUX
    // KDAB_TODO
#else
    char path[MAXPATHLEN];
    unsigned int pathSize = MAXPATHLEN;
    if (_NSGetExecutablePath(path, &pathSize) == 0)
    {
        QString sourceProcPath = path;
        QString targetProcPath = QFileInfo(path).dir().absoluteFilePath(GRIDHUB_TSR_NAME);
        QFile::remove(targetProcPath);
        if (QFile::copy(sourceProcPath, targetProcPath))
        {
            QProcess::startDetached(targetProcPath);
        }
        else
        {
            if (!failSilently)
            {
                QMessageBox::critical(nullptr, QString(), QGridHubApplication::tr("Failed to copy GridHub. Make sure that %1 is writable!"));
            }
        }
    }
#endif
}

//=========================================================================
// RelaunchImage
// [10/19/2016]
//=========================================================================
void RelaunchImage()
{
#ifdef AZ_PLATFORM_WINDOWS
    TCHAR myFileName[MAX_PATH] = { _T(0) };
    if (GetModuleFileName(NULL, myFileName, MAX_PATH))
    {
        TCHAR targetProcPath[MAX_PATH] = { _T(0) };
        TCHAR procDrive[MAX_PATH] = { _T(0) };
        TCHAR procDir[MAX_PATH] = { _T(0) };
        TCHAR procFname[MAX_PATH] = { _T(0) };
        TCHAR procExt[MAX_PATH] = { _T(0) };
        _tsplitpath_s(myFileName, procDrive, procDir, procFname, procExt);
        _tmakepath_s(targetProcPath, procDrive, procDir, GRIDHUB_IMAGE_NAME, NULL);

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL;

        CreateProcess(
            NULL,                       // No module name (use command line).
            targetProcPath,             // Command line.
            NULL,                       // Process handle not inheritable.
            NULL,                       // Thread handle not inheritable.
            FALSE,                      // Set handle inheritance to FALSE.
            CREATE_NEW_PROCESS_GROUP,   // Don't use the same process group as the current process!
            NULL,                       // Use parent's environment block.
            NULL,                       // Startup in the same directory as the executable
            &si,                        // Pointer to STARTUPINFO structure.
            &pi);                       // Pointer to PROCESS_INFORMATION structure.
    }
#elif defined AZ_PLATFORM_LINUX
    // KDAB_TODO
#else
    char path[MAXPATHLEN];
    unsigned int pathSize = MAXPATHLEN;
    if (_NSGetExecutablePath(path, &pathSize) == 0)
    {
        QProcess::startDetached(path);
    }
#endif
}

//=========================================================================
// main
// [10/24/2012]
//=========================================================================
int main(int argc, char *argv[])
{
    bool failSilently = false;

    // We are laucnhing this from Woodpecker all the time.
    // When launching from Woodpecker, we don't want to show
    // any of our error messages, which might be useful at other times.
    // So, it passes along this fail_silently flag which lets us suppress
    // our messages without disrupting any other flow.
    for (int i=0; i < argc; ++i)
    {
        if (strcmp(argv[i],"-fail_silently") == 0)
        {
            failSilently = true;
        }
    }

    bool isCopyAndRunOnExit = false;

    if( IsDebuggerPresent() == false )
    {

#ifdef AZ_PLATFORM_WINDOWS
        TCHAR exeFileName[MAX_PATH];
        if( GetModuleFileName(NULL,exeFileName,AZ_ARRAY_SIZE(exeFileName)) )
#elif defined AZ_PLATFORM_LINUX
        //KDAB_TODO
        char exeFileName[MAXPATHLEN];
#else
        char exeFileName[MAXPATHLEN];
        unsigned int pathSize = MAXPATHLEN;
        if (_NSGetExecutablePath(exeFileName, &pathSize) == 0)
#endif
        {
            if (!QString(exeFileName).contains(GRIDHUB_TSR_SUFFIX))
            {
                // this is a first time we run we need to run copy and run tool.
                isCopyAndRunOnExit = true;
            }
        }
    }

    if (isCopyAndRunOnExit == false)     // if we need to exit and run copy and run, just go there
    {
        bool isNeedToRelaunch = false;
        
        {
#ifdef AZ_PLATFORM_WINDOWS
            // Create a OS named mutex while the OS is running
            HANDLE hInstanceMutex = CreateMutex(NULL,TRUE,"Global\\GridHub-Instance");
            AZ_Assert(hInstanceMutex!=NULL,"Failed to create OS mutex [GridHub-Instance]\n");
            if( hInstanceMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
            {
                return 0;
            }
#else
            {
                QSharedMemory fix("Global\\GridHub-Instance");
                fix.attach();
            }
            // Create a OS named shared memory segment while the OS is running
            QSharedMemory mem("Global\\GridHub-Instance");
            bool created = mem.create(32);
            AZ_Assert(created,"Failed to create OS mutex [GridHub-Instance]\n");
            if( !created )
            {
                return 0;
            }
#endif

            // tell Qt where to find its plugins.
            // you can't USE qt to tell qt where to find them.
#ifdef AZ_PLATFORM_WINDOWS
            char myFileName[MAX_PATH] = {0};
            if (GetModuleFileNameA(NULL, myFileName, MAX_PATH))
            {

                char searchPath[MAX_PATH] = {0};
                char driveName[MAX_PATH] = {0};
                char directoryName[MAX_PATH] = {0};
                ::_splitpath_s( myFileName, driveName, MAX_PATH, directoryName, MAX_PATH, NULL, 0, NULL, 0);

                azstrcat(searchPath, MAX_PATH, driveName);
                azstrcat(searchPath, MAX_PATH, directoryName);
                azstrcat(searchPath, MAX_PATH, "qtlibs\\plugins");
                QApplication::addLibraryPath(searchPath);
            }
#elif defined AZ_PLATFORM_LINUX
            //KDAB_TODO
#else
            char path[MAXPATHLEN];
            unsigned int pathSize = MAXPATHLEN;
            if (_NSGetExecutablePath(path, &pathSize) == 0)
            {
                char* dirPath = dirname(path);
                QApplication::addLibraryPath(QDir(dirPath).absoluteFilePath("../../../qtlibs/plugins"));
                QApplication::addLibraryPath(QDir(dirPath).absoluteFilePath("qtlibs/plugins"));
            }
#endif

            QGridHubApplication qtApp(argc, argv);
            qtApp.Initialize();
            qtApp.Execute();
            isNeedToRelaunch = qtApp.IsNeedToRelaunch();
            qtApp.Finalize();
#ifdef AZ_PLATFORM_WINDOWS
            ReleaseMutex(hInstanceMutex);
#endif
        }

        if (isNeedToRelaunch)
        {
            // Launch the original image, which will take care
            // of overwriting us and relaunch us in turn.
            RelaunchImage();
        }
    }
    else
    {
        // We may have been launched by the TSR due to image change,
        // so wait a little bit to give the TSR time to shutdown.
        AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(500));
        CopyAndRun(failSilently);
    }

    return 0;
}
