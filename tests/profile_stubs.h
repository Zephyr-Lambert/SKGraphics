/* Injectable INI readers for host tests. */
static U (*profile_names)(W*,U,const W*);
static U (*profile_section)(const W*,W*,U,const W*);
static U (*profile_string)(const W*,const W*,const W*,W*,U,const W*);
U API GetPrivateProfileStringW(const W*a,const W*b,const W*def,W*out,U cap,const W*p){if(profile_string)return profile_string(a,b,def,out,cap,p);U i=0;while(def[i]&&i+1<cap){out[i]=def[i];i++;}out[i]=0;return i;}
U API GetPrivateProfileSectionNamesW(W*out,U cap,const W*p){if(profile_names)return profile_names(out,cap,p);out[0]=out[1]=0;return 0;}
U API GetPrivateProfileSectionW(const W*a,W*out,U cap,const W*p){if(profile_section)return profile_section(a,out,cap,p);out[0]=out[1]=0;return 0;}
