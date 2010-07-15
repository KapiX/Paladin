#include "Paladin.h"

#include <Alert.h>
#include <Locker.h>
#include <getopt.h>
#include <Message.h>
#include <Messenger.h>
#include <Mime.h>
#include <Node.h>
#include <Path.h>
#include <Roster.h>
#include <stdlib.h>
#include <String.h>
#include <TranslationUtils.h>

#include "AboutWindow.h"
#include "DebugTools.h"
#include "DPath.h"
#include "FileUtils.h"
#include "Globals.h"
#include "MsgDefs.h"
#include "ObjectList.h"
#include "PLocale.h"
#include "Project.h"
#include "ProjectBuilder.h"
#include "ProjectWindow.h"
#include "Settings.h"
#include "SourceFile.h"
#include "StartWindow.h"
#include "TemplateWindow.h"
#include "TypedRefFilter.h"


BPoint gProjectWindowPoint;
static int sReturnCode = 0;

static int32 sWindowCount = 0;
static BLocker sWindowLocker;
volatile int32 gQuitOnZeroWindows = 1;

void
RegisterWindow(void)
{
	sWindowLocker.Lock();
	sWindowCount++;
	sWindowLocker.Unlock();
}


void
DeregisterWindow(void)
{
	bool quit = false;
	sWindowLocker.Lock();
	sWindowCount--;
	
	if (sWindowCount == 0)
		quit = true;
	
	sWindowLocker.Unlock();
	if (quit)
	{
		if (gQuitOnZeroWindows)
			be_app->PostMessage(B_QUIT_REQUESTED);
		else
			gQuitOnZeroWindows = 1;
	}
}


int32
CountRegisteredWindows(void)
{
	int32 count;
	sWindowLocker.Lock();
	count = sWindowCount;
	sWindowLocker.Unlock();
	return count;
}


void
PrintUsage(void)
{
	#ifdef USE_TRACE_TOOLS
	printf("Usage: Paladin [-b] [-r] [-s] [-d] [-v] [file1 [file2 ...]]\n"
			"-b, Build the specified project. Only one file can be specified with this switch.\n"
			"-r, Completely rebuild the project\n"
			"-s, Use only one thread for building\n"
			"-d, Print debugging output\n"
			"-v, Make debugging mode verbose\n");
	#else
	printf("Usage: Paladin [-b] [-r] [-s] [file1 [file2 ...]]\n"
			"-b, Build the specified project. Only one file can be specified with this switch.\n"
			"-r, Completely rebuild the project\n"
			"-s, Use only one thread for building\n");
	#endif
}

App::App(void)
	:	BApplication(APP_SIGNATURE),
		fBuildMode(false),
		fBuildCleanMode(false),
		fBuilder(NULL),
		fProjectFilter(NULL)
{
	InitFileTypes();
	InitGlobals();
	
	gProjectList = new LockableList<Project>(20,true);
	gProjectWindowPoint.Set(5,24);

	if ((gPlatform != PLATFORM_HAIKU && gPlatform != PLATFORM_HAIKU_GCC4))
		fProjectFilter = new TypedRefFilter(PROJECT_MIME_TYPE);
	
	BMessenger msgr(this);
	BEntry entry(gLastProjectPath.GetFullPath());
	entry_ref ref;
	entry.GetRef(&ref);
	fOpenPanel = new BFilePanel(B_OPEN_PANEL,&msgr,&ref,B_FILE_NODE,true,
								new BMessage(B_REFS_RECEIVED),fProjectFilter);
	fOpenPanel->Window()->SetTitle("Paladin: Open Project");
}


App::~App(void)
{
	gSettings.Save();
	
	delete fBuilder;
	delete fOpenPanel;
	delete fProjectFilter;
}


void
App::AboutRequested(void)
{
	AboutWindow *win = new AboutWindow();
	win->Show();
}


void
App::ArgvReceived(int32 argc,char **argv)
{
	bool showUsage = false;
	bool verbose = false;
	int32 i = 1;
	for (i = 1; i < argc; i++)
	{
		int arglen = strlen(argv[i]);
		char *arg = argv[i];
		
		char opt;
		if (arglen == 2 && arg[0] == '-')
			opt = arg[1];
		else
			break;
			
		switch (opt)
		{
			case 'b':
			{
				fBuildMode = true;
				break;
			}
			case 'r':
			{
				fBuildMode = true;
				fBuildCleanMode = true;
				break;
			}
			case 's':
			{
				gSingleThreadedBuild = true;
				break;
			}
			
			#ifdef USE_TRACE_TOOLS
			case 'v':
			{
				verbose = true;
				break;
			}
			case 'd':
			{
				gPrintDebugMode = 1;
				break;
			}
			#endif
			
			default:
			{
				showUsage = true;
				break;
			}
		}
	}
	
	
	if (gPrintDebugMode > 0 && verbose)
		gPrintDebugMode = 2;
	
	if (showUsage)
	{
		PrintUsage();
		
		// A quick hack to not show the start window before
		// the quit request has been processed
		sWindowCount++;
		
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	
	if (gPrintDebugMode == 1)
		printf(TR("Printing debug output\n"));
	else if (gPrintDebugMode == 2)
		printf(TR("Printing debug output with extra detail\n"));
	
	if (gSingleThreadedBuild)
		STRACE(1,("Disabling multithreaded project building\n"));

		
	BMessage refmsg;
	int32 refcount = 0;

	// No initialization -- keep the value from the previous loop
	BEntry projfolder("/boot/home/projects");
	entry_ref projref;
	projfolder.GetRef(&projref);
	for (; i < argc; i++)
	{
		BEntry entry(argv[i]);
		entry_ref ref;
		
		if (!entry.Exists())
		{
			ref = FindFile(projref,argv[i]);
			if (!ref.name)
			{
				printf(TR("Can't find file %s\n"),argv[i]);
				continue;
			}
		}
		else
			entry.GetRef(&ref);
		refmsg.AddRef("refs",&ref);
		refcount++;
		optind++;
		
		if (refcount == 1 && fBuildMode)
			break;
	}
	
	if (refcount > 0)
		RefsReceived(&refmsg);
	else
		if (fBuildMode)
			Quit();
}


void
App::RefsReceived(BMessage *msg)
{
	entry_ref ref;
	int32 i = 0;
	while (msg->FindRef("refs",i,&ref) == B_OK)
	{
		if (Project::IsProject(ref))
		{
			if (fBuildMode)
				BuildProject(ref);
			else
				LoadProject(ref);
		}
		else
			OpenFile(ref);
		i++;
	}
}


bool
App::QuitRequested(void)
{
	gSettings.SetString("lastprojectpath",gLastProjectPath.GetFullPath());
	return true;
}


void
App::ReadyToRun(void)
{
	if (CountRegisteredWindows() < 1 && !fBuildMode)
	{
		StartWindow *win = new StartWindow();
		win->Show();
	}
}


void
App::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case M_MAKE_PROJECT:
		case M_RUN_PROJECT:
		case M_RUN_IN_TERMINAL:
		case M_RUN_IN_DEBUGGER:
		case M_RUN_WITH_ARGS:
		case M_FORCE_REBUILD:
		case M_SHOW_ADD_NEW_PANEL:
		case M_SHOW_FIND_AND_OPEN_PANEL:
		case M_SHOW_ERROR_WINDOW:
		case M_TOGGLE_ERROR_WINDOW:
		{
			entry_ref ref;
			if (msg->FindRef("refs",&ref) == B_OK)
				PostToProjectWindow(msg,&ref);
			else
				PostToProjectWindow(msg,NULL);
			
			break;
		}
		case M_OPEN_PARTNER:
		{
			entry_ref ref;
			if (msg->FindRef("refs",&ref) == B_OK)
				OpenPartner(ref);
			break;
		}
		case M_NEW_PROJECT:
		{
			TemplateWindow *win = new TemplateWindow(BRect(100,100,400,300));
			win->Show();
			break;
		}
		case M_SHOW_OPEN_PROJECT:
		{
			fOpenPanel->Show();
			break;
		}
		case M_CREATE_PROJECT:
		{
			BString name, target, path;
			int32 type, scmtype;
			bool createFolder;
			if (msg->FindString("name",&name) == B_OK &&
				msg->FindString("target",&target) == B_OK &&
				msg->FindInt32("type",&type) == B_OK &&
				msg->FindString("path",&path) == B_OK &&
				msg->FindInt32("scmtype", &scmtype) == B_OK &&
				msg->FindBool("createfolder",&createFolder) == B_OK)
			{
				Project *proj = CreateNewProject(name.String(),target.String(),
												type,path.String(),scmtype,
												createFolder);
				
				entry_ref addRef;
				int32 i = 0;
				while (msg->FindRef("libs",i++,&addRef) == B_OK)
				{
					if (BEntry(&addRef).Exists())
						proj->AddLibrary(DPath(addRef).GetFullPath());
				}
				
				i = 0;
				BMessage addMsg(M_IMPORT_REFS);
				while (msg->FindRef("refs",i++,&addRef) == B_OK)
					addMsg.AddRef("refs",&addRef);
				PostToProjectWindow(&addMsg,NULL);
			}
			break;
		}
		case M_QUICK_IMPORT:
		{
			entry_ref ref;
			if (msg->FindRef("refs",&ref) != B_OK || !QuickImportProject(DPath(ref)))
			{
				StartWindow *startwin = new StartWindow();
				startwin->Show();
				break;
			}
			break;
		}
		
		// These are for quit determination. We have to use our own counter variable
		// (sWindowCount) because BFilePanels throw the count off. Using a variable
		// is much preferable to subclassing just for this reason.
		case M_REGISTER_WINDOW:
		{
			sWindowCount++;
			break;
		}
		case M_DEREGISTER_WINDOW:
		{
			sWindowCount--;
			if (sWindowCount <= 1)
				PostMessage(B_QUIT_REQUESTED);
			break;
		}
		case PALEDIT_OPEN_FILE:
		{
			int32 index = 0;
			entry_ref ref;
			while (msg->FindRef("refs",index,&ref) == B_OK)
			{
				int32 line;
				if (msg->FindInt32("line",index,&line) != B_OK)
					line = -1;
				
				OpenFile(ref,line);
				
				index++;
			}
			
			fOpenPanel->GetPanelDirectory(&ref);
			gLastProjectPath.SetTo(ref);
			break;
		}
		case M_FIND_AND_OPEN_FILE:
		{
			FindAndOpenFile(msg);
			break;
		}
		case M_BUILDING_FILE:
		{
			SourceFile *file;
			if (msg->FindPointer("sourcefile",(void**)&file) == B_OK)
				printf(TR("Building %s\n"),file->GetPath().GetFileName());
			break;
		}
		case M_LINKING_PROJECT:
		{
			printf(TR("Linking\n"));
			break;
		}
		case M_UPDATING_RESOURCES:
		{
			printf(TR("Updating Resources\n"));
			break;
		}
		case M_BUILD_FAILURE:
		case M_BUILD_WARNINGS:
		{
			BString errstr;
			if (msg->FindString("errstr",&errstr) == B_OK)
				printf("%s\n",errstr.String());
			sReturnCode = -1;
			PostMessage(B_QUIT_REQUESTED);
			break;
		}
		case M_BUILD_SUCCESS:
		{
			printf(TR("Success\n"));
			PostMessage(B_QUIT_REQUESTED);
			break;
		}
		default:
			BApplication::MessageReceived(msg);
	}
}


void
App::OpenFile(entry_ref ref, int32 line)
{
	if (!ref.name)
		return;
	
	BNode node(&ref);
	BString type;
	if (node.ReadAttrString("BEOS:TYPE",&type) != B_OK)
	{
		update_mime_info(BPath(&ref).Path(),0,0,1);
		node.ReadAttrString("BEOS:TYPE",&type);
	}
	
	if (type.CountChars() > 0)
	{
		if (type == PROJECT_MIME_TYPE || type.FindFirst("text/") != 0)
		{
			be_roster->Launch(&ref);
			return;
		}
	}
	else
	{
		BString extension = BPath(&ref).Path();
		int32 pos = extension.FindLast(".");
		if (pos >= 0)
		{
			extension = extension.String() + pos;
			if (extension.ICompare(".cpp") != 0 &&
				extension.ICompare(".h") != 0 &&
				extension.ICompare(".hpp") != 0 &&
				extension.ICompare(".c") != 0)
			{
				return;
			}
		}
	}
	
//	BMessage msg(B_REFS_RECEIVED);
	BMessage msg(PALEDIT_OPEN_FILE);
	msg.AddRef("refs",&ref);
	if (line >= 0)
		msg.AddInt32("line",line);
	
	if (be_roster->IsRunning(EDITOR_SIGNATURE))
	{
		BMessenger msgr(EDITOR_SIGNATURE);
		msgr.SendMessage(&msg);
	}
	else
	{
		DPath path(gAppPath.GetFolder());
		path.Append("PalEdit");
		
		entry_ref launchref;
		BEntry(path.GetFullPath()).GetRef(&launchref);
		
		if (be_roster->Launch(&launchref,&msg) != B_OK &&
			be_roster->Launch(EDITOR_SIGNATURE,&msg) != B_OK)
			be_roster->Launch(&ref);
	}
}


void
App::OpenPartner(entry_ref ref)
{
	DPath refpath(BPath(&ref).Path());
	BString ext(refpath.GetExtension());
	if (ext.CountChars() < 1)
		return;
	
	BString pathbase = refpath.GetFullPath();
	pathbase.Truncate(pathbase.FindLast(".") + 1);
	
	const char *cpp_ext[] = { "cpp","c","cxx","cc", NULL };
	const char *hpp_ext[] = { "h","hpp","hxx","hh", NULL };
	
	bool is_source = false;
	bool is_header = false;
	
	int i = 0;
	while (cpp_ext[i])
	{
		if (ext == cpp_ext[i])
		{
			is_source = true;
			break;
		}
		i++;
	}
	
	if (!is_source)
	{
		i = 0;
		while (hpp_ext[i])
		{
			if (ext == hpp_ext[i])
			{
				is_header = true;
				break;
			}
			i++;
		}
	}
	else
	{
		i = 0;
		while (hpp_ext[i])
		{
			BString partpath = pathbase;
			partpath << hpp_ext[i];
			BEntry entry(partpath.String());
			if (entry.Exists())
			{
				entry_ref header_ref;
				entry.GetRef(&header_ref);
				OpenFile(header_ref);
				return;
			}
			i++;
		}
	}
	
	if (is_header)
	{
		i = 0;
		while (cpp_ext[i])
		{
			BString partpath = pathbase;
			partpath << cpp_ext[i];
			BEntry entry(partpath.String());
			if (entry.Exists())
			{
				entry_ref source_ref;
				entry.GetRef(&source_ref);
				OpenFile(source_ref);
				return;
			}
			i++;
		}
	}
	
	BString errmsg;
	if (is_source)
		errmsg	<< "Couldn't find a corresponding header file for " << ref.name
				<< " in " << refpath.GetFolder() << "/ .";
	else
	if (is_header)
		errmsg	<< "Couldn't find a corresponding source file for " << ref.name
				<< " in " << refpath.GetFolder() << "/ .";
	else
		errmsg	<< 	TR("You can only find the partner file for C/C++ files and their "
					"headers.");
	BAlert *alert = new BAlert("Paladin",errmsg.String(),"OK");
	alert->Go();
}


Project *
App::CreateNewProject(const char *projname, const char *target,	int32 type,
						const char *path, int32 scmType, bool create_folder)
{
	Project *proj = Project::CreateProject(projname,target,type,path,create_folder);
	if (!proj)
		return NULL;
	
	proj->SetSourceControl((scm_t)scmType);
	
	gCurrentProject = proj;
	gProjectList->Lock();
	gProjectList->AddItem(proj);
	gProjectList->Unlock();
	
	BRect r(0,0,200,300);
	r.OffsetTo(gProjectWindowPoint);
	gProjectWindowPoint.x += 25;
	gProjectWindowPoint.y += 25;
	ProjectWindow *projwin = new ProjectWindow(r,gCurrentProject);
	projwin->Show();
	
	BEntry entry(gCurrentProject->GetPath().GetFullPath());
	if (entry.InitCheck() == B_OK)
	{
		entry_ref newprojref;
		entry.GetRef(&newprojref);
		UpdateRecentItems(newprojref);
	}
	
	return proj;
}


bool
App::QuickImportProject(DPath folder)
{
	// Quickly makes a project in a folder by importing all resource files and C++ sources.
	if (!BEntry(folder.GetFullPath()).Exists())
		return false;
	
	DPath path(folder);
	BEntry entry(path.GetFullPath());
	Project *proj = CreateNewProject(folder.GetFileName(),folder.GetFileName(),
									PROJECT_GUI,folder.GetFullPath(),gDefaultSCM,
									false);
	if (!proj)
		return false;
	
	entry.SetTo(folder.GetFullPath());
	entry_ref addref;
	entry.GetRef(&addref);
	BMessage addmsg(M_ADD_FILES);
	addmsg.AddRef("refs",&addref);
	
	PostToProjectWindow(&addmsg,NULL);
	return true;
}


void
App::BuildProject(const entry_ref &ref)
{
	BPath path(&ref);
	Project *proj = new Project;
	
	if (proj->Load(path.Path()) != B_OK)
	{
		BMessage msg(M_BUILD_FAILURE);
		PostMessage(&msg);
		
		delete proj;
		return;
	}
	
	if (proj->IsReadOnly())
	{
		BString err(path.Path());
		err << TR(" is on a read-only disk. Please copy the project to another disk ")
			<<	TR("or remount the disk with write support to be able to build it.\n");
		BMessage msg(M_BUILD_FAILURE);
		msg.AddString("errstr",err);
		PostMessage(&msg);
		
		delete proj;
		return;
	}
	
	gProjectList->Lock();
	gProjectList->AddItem(proj);
	gProjectList->Unlock();
	
	gCurrentProject = proj;
	if (!fBuilder)
		fBuilder = new ProjectBuilder(BMessenger(this));
	
	if (fBuildCleanMode)
		proj->ForceRebuild();
	
	fBuilder->BuildProject(proj, POSTBUILD_NOTHING);
}


void
App::LoadProject(const entry_ref &ref)
{
	if (!ref.name)
		return;
	
	for (int32 i = 0; i < CountWindows(); i++)
	{
		ProjectWindow *win = dynamic_cast<ProjectWindow*>(WindowAt(i));
		if (!win)
			continue;
		Project *winProject = win->GetProject();
		if (!winProject)
			continue;
		BEntry entry(winProject->GetPath().GetFullPath());
		if (entry.InitCheck() != B_OK)
			continue;
		entry_ref projRef;
		entry.GetRef(&projRef);
		if (ref == projRef)
			return;
	}
	
	BPath path(&ref);
	Project *proj = new Project;
	
	if (proj->Load(path.Path()) != B_OK)
	{
		delete proj;
		return;
	}
	
	UpdateRecentItems(ref);
	
	gCurrentProject = proj;
	gProjectList->Lock();
	gProjectList->AddItem(proj);
	gProjectList->Unlock();
	
	BRect r(0,0,200,300);
	r.OffsetTo(gProjectWindowPoint);
	gProjectWindowPoint.x += 25;
	gProjectWindowPoint.y += 25;
	ProjectWindow *projwin = new ProjectWindow(r,gCurrentProject);
	projwin->Show();
	
	if (proj->IsReadOnly())
	{
		BString errmsg;
		errmsg << TR("This project is on a read-only disk. You will not be able ");
		errmsg << TR("to build it, but you can still view its files and do anything ");
		errmsg << TR("else that does not require saving to the disk. ");
		BAlert *alert = new BAlert("Paladin",errmsg.String(), "OK");
		alert->Go();
	}
}


void
App::UpdateRecentItems(const entry_ref &newref)
{
	gSettings.Lock();
	
	entry_ref ref;
	int32 index = 0;
	while (gSettings.FindRef("recentitems",index++,&ref) == B_OK)
	{
		
		if (ref == newref)
		{
			gSettings.RemoveData("recentitems",index - 1);
			break;
		}
	}
	gSettings.AddRef("recentitems",&newref);
	
	gSettings.Unlock();
}


void
App::PostToProjectWindow(BMessage *msg, entry_ref *file)
{
	if (!msg)
		return;
	
	BPath path(file);
	
	for (int32 i = 0; i < CountWindows(); i++)
	{
		ProjectWindow *win = dynamic_cast<ProjectWindow*>(WindowAt(i));
		if (!win)
			continue;
		if ( (file && win->GetProject()->HasFile(path.Path())) ||
				win->GetProject() == gCurrentProject)
		{
			if (win->AreMenusLocked() && win->RequiresMenuLock(msg->what))
				break;
			win->Activate(true);
			win->PostMessage(msg);
			break;
		}
	}
}


BWindow *
WindowForProject(Project *proj)
{
	if (!proj)
		return NULL;
	
	
	for (int32 i = 0; i < be_app->CountWindows(); i++)
	{
		ProjectWindow *win = dynamic_cast<ProjectWindow*>(be_app->WindowAt(i));
		if (!win)
			continue;
		
		if (win->GetProject() == proj)
			return win;
	}
	return NULL;
}


int
main(int argc, char **argv)
{
	if (argc > 1 && (strcmp(argv[1],"--help") == 0 || strcmp(argv[1],"-h") == 0) )
	{
		PrintUsage();
		return 0;
	}
	else
	{
		// Initialize localization under Haiku
//		#ifdef __HAIKU__
//		BCatalog cat;
//		be_locale->GetAppCatalog(&cat);
//		#endif
		
		App().Run();
	}
	
	return sReturnCode;
}
