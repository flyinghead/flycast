#pragma once

template <class T>
struct List
{
	T* daty;
	int avail;

	int size;
	bool* overrun;
	const char *list_name;

	__forceinline int used() const { return size-avail; }
	__forceinline int bytes() const { return used()* sizeof(T); }

	NOINLINE
	T* sig_overrun() 
	{ 
		*overrun |= true;
		Clear();
		if (list_name != NULL)
			printf("List overrun for list %s\n", list_name);

		return daty;
	}

	__forceinline 
	T* Append(int n=1)
	{
		int ad=avail-n;

		if (ad>=0)
		{
			T* rv=daty;
			daty+=n;
			avail=ad;
			return rv;
		}
		else
			return sig_overrun();
	}

	__forceinline 
	T* LastPtr(int n=1) 
	{ 
		return daty-n; 
	}

	T* head() const { return daty-used(); }

	void InitBytes(int maxbytes,bool* ovrn, const char *name)
	{
		maxbytes-=maxbytes%sizeof(T);

		daty=(T*)malloc(maxbytes);
		
		verify(daty!=0);

		avail=size=maxbytes/sizeof(T);

		overrun=ovrn;

		Clear();
		list_name = name;
	}

	void Init(int maxsize,bool* ovrn, const char *name)
	{
		InitBytes(maxsize*sizeof(T),ovrn, name);
	}

	void Clear()
	{
		daty=head();
		avail=size;
	}

	void Free()
	{
		Clear();
		free(daty);
	}
};
