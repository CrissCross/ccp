# Documentation
## Generate a html documentation
```
$ ./gen_html_doku.sh
```
## Generate a pdf documentation
1. Generate html documentation as indicated above
2. Use the tool [wkhtmltopdf](http://code.google.com/p/wkhtmltopdf/) to generate a pdf out of it.
```
$ wkhtmltopdf -s A4 doku.html doku.odf
```
## Download a possibly outdated pdf documentation
http://dublin.zhaw.ch/~gehricri/ccp/doku/doku.pdf
