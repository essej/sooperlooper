#
# autoconf macro to remove duplicated elements in a list, but to leave
# in place only the *last* occurence of each duplicate element.
# Intended for use with lists of -l args, and the like.
#
# usage: AC_UNIQUIFY_LAST([list],[shell-var-to-set-to-uniquified-list])

AC_DEFUN([AC_UNIQUIFY_LAST],
[
AC_REQUIRE([AC_PROG_AWK])
changequote(<,>)dnl
ac_u_result=`$AWK 'BEGIN {
		          for (i = 1; length(ARGV[i]); i++) {
			    cnt[ARGV[i]]++;
			  }
			  for (i = 1; length(ARGV[i]); i++) {
			    if (--cnt[ARGV[i]] == 0) {
			       printf ("%s ", ARGV[i]);
			    }
			  }
			  print; exit 0}' $1`
changequote([,])dnl
	$2="$ac_u_result"
])	

#
# autoconf macro to remove duplicated elements in a list, but to leave
# in place only the *first* occurence of each duplicate element.
# Intended for use with lists of -I args, and the like.
#
# usage: AC_UNIQUIFY_FIRST([list],[shell-var-to-set-to-uniquified-list])

AC_DEFUN([AC_UNIQUIFY_FIRST],
[
AC_REQUIRE([AC_PROG_AWK])
changequote(<,>)dnl
ac_u_result=`$AWK 'BEGIN {
			  for (i = 1; length(ARGV[i]); i++) {
			    if (cnt[ARGV[i]] == 0) {
			       printf ("%s ", ARGV[i]);
			       cnt[ARGV[i]]++;
			    }
			  }
			  print; exit 0}' $1`
changequote([,])dnl
	$2="$ac_u_result"
])	
