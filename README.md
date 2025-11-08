# otus

Lightweight system monitor built with **FTXUI**. Shows CPU/GPU/MEM/DISK, plus a grouped, hierarchical process view.

Please note GPU support is limited in current version. Check out devnotes.md for planned release features


## Clone & Build 
1.Clone 
```bash
git clone https://github.com/Sadish09/otus
cd otus 
```  
2.Build 
```bash
mkdir build && cd build 
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j 
./otus #opens the default dashboard 
```

## Usage Examples 

`otus` 

![otus_default](screenshots/otus_default)

`otus -cpu` 

![ss for cpu](screenshots/otus_cpu)

`otus -mem` 

![ss for mem](screenshots/otus_mem)

`otus -proc` 

![ss for proc](screenshots/otus_proc) 

`otus -proc -lim N(default 40)` 

![ss for proc lim](screenshots/otus_lim) 

