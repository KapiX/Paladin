#ifndef SOURCE_TYPE_C_H
#define SOURCE_TYPE_C_H

#include "ErrorParser.h"
#include "SourceFile.h"
#include "SourceType.h"

class SourceTypeC : public SourceType
{
public:
						SourceTypeC(void);
			int32		CountExtensions(void) const;
			BString		GetExtension(const int32 &index);
	
			SourceFile *		CreateSourceFile(const char *path);
			SourceOptionView *	CreateOptionView(void);
			BString		GetName(void) const;

private:
			
};

class SourceFileC : public SourceFile
{
public:
						SourceFileC(const char *path);
			bool		UsesBuild(void) const;
			void		UpdateDependencies(BuildInfo &info);
			bool		CheckNeedsBuild(BuildInfo &info, bool check_deps = true);
			void		Compile(BuildInfo &info, const char *options);
	
			DPath		GetObjectPath(BuildInfo &info);
			void		RemoveObjects(BuildInfo &info);
};

#endif