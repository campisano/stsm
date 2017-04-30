# defining common functions
utils = new.env(hash=TRUE, parent=emptyenv());



utils$setVerbose = function() {
    # defining common starting options
    options(download.file.method = "wget");
    options(warn=1);  # warnings printed as they occur
    options(keep.source=TRUE);
    options(error=quote({
        # from http://stackoverflow.com/a/2000757/846686
        cat("Environment:\n", file=stderr());
        dump.frames();
        n = length(last.dump);
        calls = names(last.dump);
        cat(paste(
            "  ", 1L:n, ": ", calls, sep = ""), sep = "\n", file=stderr());
        cat("\n", file=stderr());

        if (!interactive()) {
            q();
        }
    }));
}



utils$loadLibs = function(lib_name_and_vers) {
    repos = "http://cran.r-project.org";
    options(repos=c(CRAN=repos));
    lib = paste(Sys.getenv("HOME"), "R", "library", sep="/");
    dir.create(lib, showWarnings=FALSE, recursive=TRUE, mode="2755");
    .libPaths(c(lib));

    # use devtools
    ; lib_name = "devtools";

    # if(!suppressWarnings(library(
    #     package=lib_name,
    #     character.only=TRUE, logical.return=TRUE, quietly=TRUE))) {
    #     install.packages(
    #         pkgs=lib_name, lib=lib, repos=repos,
    #         quiet=FALSE, dependencies=TRUE);
    #     library(
    #         lpackage=lib_name,
    #         character.only=TRUE, quietly=TRUE);
    # }

    # lib_names = c(lib_name);
    lib_names = c();

    # install specific libs
    for(lib_name_and_ver in lib_name_and_vers) {
        lib_name = unlist(strsplit(lib_name_and_ver, ":"))[1];
        lib_ver = unlist(strsplit(lib_name_and_ver, ":"))[2];
        lib_names = c(lib_names, lib_name);

        if(!suppressWarnings(library(
            package=lib_name,
            character.only=TRUE, logical.return=TRUE, quietly=TRUE))) {
            #install.versions(
            #    lib_name, versions=lib_ver, repos=repos, lib=lib,
            #    quiet=FALSE, dependencies=TRUE);
            #install_version(
            #    lib_name, version=lib_ver, repos=repos, lib=lib,
            #    quiet=FALSE, dependencies=TRUE);
            install.packages(
                pkgs=lib_name, lib=lib,
                repos=repos,
                quiet=FALSE, dependencies=TRUE);
            library(
                package=lib_name,
                character.only=TRUE, quietly=TRUE);
        }
    }

    return(lib_names);
}



# TO TEST
utils$loadLibsPath = function(lib_paths) {
    repos = "http://cran.r-project.org";
    options(repos=c(CRAN=repos));
    lib = paste(Sys.getenv("HOME"), "R", "library", sep="/");
    dir.create(lib, showWarnings=FALSE, recursive=TRUE, mode="2755");
    .libPaths(c(lib));

    # install specific libs
    for(lib_path in lib_paths) {
        install.packages(
            pkgs=lib_path, repos=NULL, type="source",
            quiet=FALSE, dependencies=TRUE);
    }
}



utils$newDict = function() {
    return(new.env(hash=TRUE, parent=emptyenv()));
}



# TO TEST
utils$dict_to_data_frame = function(dict, col_names) {
    new_data = list();

    for(col_name in col_names) {
        new_data[[col_name]] = c();
    }

    for(key in ls(dict)) {
        for(col_name in col_names) {
            new_data[[col_name]] = c(
                new_data[[col_name]], dict[[key]][[col_name]]);
        }
    }

    data_frame = data.frame(col_names);
    colnames(data_frame) = col_names;

    return(data_frame);
}



utils$add_extension = function(path, ext) {
    return(paste(path, ext, sep="."));
}



utils$remove_extension = function(path) {
    return(sub("[.][^.]*$", "", path, perl=TRUE));
}



utils$change_extension = function(path, ext) {
    return(
        utils$add_extension(
            utils$remove_extension(path), ext));
}



utils$readCSV = function(file_name, header=TRUE) {
    data = read.table(
        file=file_name, header=header, fill=TRUE, as.is=TRUE,
        stringsAsFactors=FALSE, sep=",", quote="");

    return(data);
}



utils$writeCSV = function(data_frame, file_name, header=TRUE) {
    write.table(
        data_frame, file=file_name, col.names=header,
        append=FALSE, row.names=FALSE, quote=FALSE, sep=",");
}



utils$readJSON = function(file_name) {
    return(fromJSON(file=file_name, method="C"));
}



utils$dev_open_file = function(
    file_name, width=480, height=480, scale=1, dpi=72
    ) {
    ext = strsplit(file_name, "\\.")[[1]][[-1]];

    pointsize = 12 * dpi / 72  # do not change

    if(ext == "svg") {
        svg(file_name, bg="transparent", antialias="none",
            pointsize=pointsize,
            width=(width / dpi) * scale, height=(height / dpi) * scale);
    }
    if(ext == "pdf") {
        pdf(file_name, bg="transparent", onefile=FALSE, title="",
            pointsize=pointsize,
            width=(width / dpi) * scale, height=(height / dpi) * scale);
    }
    if(ext == "png") {
        png(file_name, bg="transparent", antialias="none",
            pointsize=pointsize, units="in", res=dpi,
            width=(width / dpi) * scale, height=(height / dpi) * scale);
    }
    if(ext == "jpg") {
        jpeg(file_name, bg="white", antialias="none",
            pointsize=pointsize, units="in", res=dpi, quality=0.33,
            width=(width / dpi) * scale, height=(height / dpi) * scale);
    }
}



utils$dev_off = function() {
    invisible(dev.off());
}



utils$bar_plot = function(
    data_frame, x_col, y_col,
    log=FALSE, title=NA, x_title=NA, y_title=NA, caption=NA) {

    # ensure to not modify original data
    data_frame = cbind(data_frame);

    # need to be a string to not considerate
    # this number as a space for the x axis
    data_frame[[x_col]] = as.character(data_frame[[x_col]]);
    bins = as.character(sort(as.numeric(unique(data_frame[[x_col]]))));

    gg = ggplot();
    gg = gg + theme_bw();
    gg = gg + scale_x_discrete(breaks=bins, limits=bins);

    if(log) {
        gg = gg + scale_y_log10(
            labels=trans_format('log10', math_format(10^.x)));
    } else {
        gg = gg + scale_y_continuous(labels=comma);
    }

    gg = gg + geom_bar(
        data=data_frame, aes_string(x=x_col, y=y_col),
        position="identity", stat="identity");

    if(!is.na(title)) {
        gg = gg + labs(title=title);
    }

    if(!is.na(x_title)) {
        gg = gg + labs(x=x_title);
    }

    if(!is.na(y_title)) {
        gg = gg + labs(y=y_title);
    }

    if(!is.na(caption)) {
        gg = gg + labs(caption=caption);
    }

    plot(gg);
}



utils$html.getHTMLpreContentCode = function(lang="en", title="") {
    return(paste(
        "<!DOCTYPE html>",
        paste("<html lang=\"", lang, "\">", sep=""),
        "  <head>",
        paste("    <title>", title, "</title>", sep=""),
        "    <style type=\"text/css\">",
        "      .container {margin: 0 auto; padding: 3px;}",
        "      .content {float: left; margin: 3px;}",
        "      .content.first {clear: left;}",
        "      .clearfix:after {",
        "          content: \".\"; display: block; height: 0;",
        "          clear: both; visibility: hidden;}",
        "    </style>",
        "  </head>",
        "  <body>",
        "    <div class=\"container\">",
        "      <div class=\"clearfix\">",
        sep="\n"));
}



utils$html.getHTMLpostContentCode = function() {
    return(paste(
        "      </div>",
        "    </div>",
        "  </body>",
        "</html>",
        sep="\n"));
}
