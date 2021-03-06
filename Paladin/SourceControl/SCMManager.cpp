/*
 * Copyright 2018 Adam Fowler <adamfowleruk@gmail.com>
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Adam Fowler, adamfowleruk@gmail.com
 */
#include "SCMManager.h"
#include "GitSourceControl.h"
#include "HgSourceControl.h"
#include "SVNSourceControl.h"

#include <Catalog.h>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SCMManager"

SourceControl *
GetSCM(const scm_t &type)
{
	SourceControl *scm = NULL;
	switch (type)
	{
		case SCM_GIT:
		{
			scm = new GitSourceControl();
			break;
		}
		case SCM_HG:
		{
			scm = new HgSourceControl();
			break;
		}
		case SCM_SVN:
		{
			scm = new SVNSourceControl();
			break;
		}
		default:
			break;
	}
	
	return scm;
}


scm_t
DetectSCM(const char *path)
{
	BEntry entry(path);
	if (entry.InitCheck() != B_OK || !entry.Exists())
		return SCM_NONE;
	
	HgSourceControl hg;
	GitSourceControl git;
	SVNSourceControl svn;
	
	if (hg.DetectRepository(path))
		return SCM_HG;
	else if (git.DetectRepository(path))
		return SCM_GIT;
	else if (svn.DetectRepository(path))
		return SCM_SVN;
	
	return SCM_NONE;
}


bool
HaveSCM(const scm_t &type)
{
	switch (type)
	{
		case SCM_GIT:
		case SCM_HG:
		case SCM_SVN:
			return true;
		default:
			return false;
	}
}


BString
SCM2LongName(const scm_t &type)
{
	switch (type)
	{
		case SCM_GIT:
			return BString(B_TRANSLATE("Git"));
		
		case SCM_HG:
			return BString(B_TRANSLATE("Mercurial"));
			
		case SCM_SVN:
			return BString(B_TRANSLATE("Subversion"));
		
		case SCM_NONE:
			return BString(B_TRANSLATE("None"));
		
		default:
			return BString(B_TRANSLATE("Unknown"));
	}
}


BString
SCM2ShortName(const scm_t &type)
{
	switch (type)
	{
		case SCM_GIT:
			return BString("git");
		
		case SCM_HG:
			return BString("hg");
			
		case SCM_SVN:
			return BString("svn");
		
		case SCM_NONE:
			return BString("none");
		
		default:
			return BString();
	}
}

