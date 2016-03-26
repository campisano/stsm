# configure detailed error outputs
options(warn=2, keep.source=TRUE);

# loading dependences
dep = "jsonlite"; if (!require(dep, character.only = TRUE, quietly = TRUE)) {
    install.packages(dep); library(dep, character.only = TRUE);
}; rm(dep);

dep = "TSMining"; if (!require(dep, character.only = TRUE, quietly = TRUE)) {
    install.packages(dep); library(dep, character.only = TRUE);
}; rm(dep);

source(file.path("R", "motif_lib.R"));
load_lib();



args = commandArgs(TRUE);
print("Arguments:");
print(args);



csv_dataset = args[1];
input_file_json = args[2];
base_json_path_dir = dirname(input_file_json);
base_path_dir = dirname(base_json_path_dir);
base_filename = sub(file.path(base_json_path_dir, ""), "", input_file_json);
base_filename =  sub("[.][^.]*$", "", base_filename, perl=TRUE);



json_data = fromJSON(file(input_file_json));

if(is.null(nrow(json_data)))
{
    print("Empty json data");
} else {
    # WARNING! getting only the 3 last sequences, of the bigger length!
    for(len in c(0, 1, 2))
    {
        sequence_data_length = json_data[nrow(json_data) - len,];
        sequence_length = sequence_data_length$first;

        if(length(sequence_data_length$second) < 1)
        {
            print(paste("Empty sequence data of len", len));
            #print("Data:");
            #dput(sequence_data_length);
            next;
        }

        sequence_data = sequence_data_length$second[[1]];  # c++ pair

        my_stmotifs = list();

        for(j in 1:nrow(sequence_data))
        {
            sequence_data_item = sequence_data[j,];
            sequence = sequence_data_item$first;
            sequence = gsub("[()<>]", "", sequence);
            support = sequence_data_item$second[1,]$first;
            match_data = sequence_data_item$second[1,]$second[[1]]; # c++ pair

            my_item = list();
            #my_item[['saxcod']] = data.frame(
            #    t(strsplit(sequence, split="")), check.names = FALSE);
            my_item[['saxcod']] = data.frame(
                t(data.frame(strsplit(sequence, split=""), row.names = NULL)),
                row.names = NULL);
            my_item[['isaxcod']] = sequence;
            my_item[['recmatrix']] = '';
            my_item[['vecst']] = data.frame(
                's'=match_data$second, 't'=match_data$first);

            my_stmotifs[[sequence]] = my_item;
        }

        # cleanup
        rm(
            my_item, match_data, support, sequence, sequence_data_item,
            sequence_data, j, sequence_length, sequence_data_length
        );

        filename_rdata = file.path(
            base_path_dir, "rdata",
            paste(paste(base_filename, len, sep="_-"), "rdata", sep=".")
        );

        # save a fast readable Rdata file
        save(my_stmotifs, file=filename_rdata);
        load(filename_rdata);




        #setwd("/home/t1t0/Desktop/mestrado/GSP")
        #load("20_10_4_5_C.RData") # candidates
        #load("t100.RData") # dataset 40x100

        dataset = read.table(
            file = csv_dataset,
            header = TRUE,
            fill = TRUE,
            as.is = TRUE,
            stringsAsFactors = FALSE,
            sep = " ",
            quote = "");
        #stmotifs = STSIdentifySTMotifs(candidates, 5, 5)
        #sttightmotifs = STSIdentifyTightSTMotifs(stmotifs, candidates$rectangles)
        #ranksttightmotifs = STSRankTightSTMotifs(sttightmotifs)

        filename_png = file.path(
            base_path_dir, "png",
            paste(paste(base_filename, len, sep="_-"), "png", sep=".")
        );

        png(filename_png, bg="transparent");
        ds = PlotSTMotifs(my_stmotifs);
        dev.off();
    }
}