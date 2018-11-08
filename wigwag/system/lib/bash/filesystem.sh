#---------------------------------------------------------------------------------------------------------------------------
# filesystem library
#---------------------------------------------------------------------------------------------------------------------------
fs_help(){
	:
}

fs_mkdirp(){
	if [[ ! -d "$1" ]]; then
		mkdir -p "$1"
	fi
}

fs_touch(){
	if [[ ! -e "$1" ]]; then
		touch "$1"
	fi
}

fs_mktempd(){
	mktemp -d
}