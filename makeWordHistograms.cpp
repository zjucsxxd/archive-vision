/*
 * ArchiveVision: Compter Vision Engine for Digital Archives
 * 
 * file: makeWordHistograms.cpp
 *
 * Reads a library of images and outputs visual word documents
 * for the library.
 *
 * Copyright (C) 2012 Carl Stahmer (cstahmer@gmail.com) 
 * Early Modern Center, University of California, Santa Barbara
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the Creative Commons licence, version 3.
 *    
 * See http://creativecommons.org/licenses/by/3.0/legalcode for the 
 * complete licence.
 *    
 * This program is distributed WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY or FITNESS 
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.
 * 
 */

#include "makeWordHistograms.h"

using namespace cv; 
using namespace std;

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::string;

char ch[30];

Mat dictionary;


int main(int argc, char* argv[]) {
	
	Helper helper;
	ImgDoc imagedoc;
	string event;
	
	//--------Using SURF as feature extractor and FlannBased for assigning a new point to the nearest one in the dictionary
	Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create("FlannBased");
	Ptr<DescriptorExtractor> extractor = new SurfDescriptorExtractor();
	SurfFeatureDetector detector(300); // original value 500 documentation says somewhere between 300-500 is good depending on sharpness and contrast
	TermCriteria tc(CV_TERMCRIT_ITER, 10, 0.001);
	int retries = 1;
	int flags = KMEANS_PP_CENTERS;
	BOWImgDescriptorExtractor bowDE(extractor, matcher);
	
	
	string evalDirectory = EVAL_DIR;
	string evalHistsDirectory = COLLECTION_VW_FILES_DIR;
	string pathToDictionary = EVAL_DICTIONARY_PATH;
	bool runInBackground = RUN_IN_BACKGROUND;
	bool writelog = WRITE_LOG;
	bool isDir = false; // set to designate if this is an evaluation directory of a single file to be evaluated

	
	//"Usage is -eval <dirctory of evaluation files> -write <directory to which histogram files should be saved> -dict <dictionary file to load>"
	
    for (int i = 1; i < argc; i++) { 
    	string arument = argv[i];
        if (arument == "-eval") {
        	evalDirectory = argv[i + 1];
        }
        if (arument == "-write") {
        	evalHistsDirectory = argv[i + 1];
        }
        if (arument == "-dict") {
        	pathToDictionary = argv[i + 1];
        }
        if (arument == "-back") {
        	runInBackground = true;
        }
        if (arument == "-log") {
        	writelog = true;
        }
        if (arument == "-help") {
            cout << "Usage: ./makeWordHistograms -eval <dirctory of evaluation files or name of evaluation file> -write <directory to which histogram files should be saved> -dict <dictionary file to load> -back [flag to run in background mode] -log [flag to run in log mode]"<<endl;
            exit(0);
        } 
    }
    
	event = "Starting makeWordHistograms execuatable.";
	helper.logEvent(event, 2, runInBackground, writelog);
    event = "Evaluation Directory/File: " + evalDirectory;
    helper.logEvent(event, 2, runInBackground, writelog);
    event = "Histograms will be saved to: " + evalHistsDirectory;
    helper.logEvent(event, 2, runInBackground, writelog);
    event = "Using Dictionary: " + pathToDictionary;
    helper.logEvent(event, 2, runInBackground, writelog);
	
	//make sure that the eval directory exists
	struct stat sb;
	const char * fc = evalDirectory.c_str();
	if (stat(fc, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		isDir = true;
	}
	if (!isDir) {
		// ok, if this isn't a directory, see if it is a single file that can be loaded/
		// ultimately should check name and see if it is a .jpg file and not just a file
		if (access (evalDirectory.c_str (), F_OK) != 0) {
			event = evalDirectory + " is not a valid directory or image file or you do not have the correct permissions!";
		    helper.logEvent(event, 0, runInBackground, writelog);
		    event = "Operation Aborted!";
		    helper.logEvent(event, 0, runInBackground, writelog);
			exit(0);
		}
	}
	
	//make sure that the dictionary exists directory exists
	if (access (pathToDictionary.c_str (), F_OK) != 0) {
		event = pathToDictionary + " is not a valid dictionary file or you do not have the correct permissions!";
	    helper.logEvent(event, 0, runInBackground, writelog);
	    event = "Operation Aborted!";
	    helper.logEvent(event, 0, runInBackground, writelog);
		exit(0);
	}

	helper.StartClock();
	//helper.PrintElapsedClock();
	
	string structureName;
	string thisDelim = "/";
	vector<string> pathPieces = helper.Split(thisDelim, pathToDictionary);
	string thisPart = pathPieces[(pathPieces.size() - 1)];
	string secondDelim = ".";
	vector<string> filenamePart = helper.Split(secondDelim, thisPart);
	string finalPart;
	if (filenamePart.size() > 1) {
		finalPart = filenamePart[(filenamePart.size() - 2)];
	} else {
		finalPart = "dictionary";
	}
	structureName = finalPart;
	
	event = "Loading Dictionary...";
	helper.logEvent(event, 2, runInBackground, writelog);
	Mat dictionary = helper.ReadMatFromFile(pathToDictionary, structureName);
	int dictionarySize = dictionary.rows;
	if (dictionarySize < 1) {
		event = pathToDictionary + " is an empty dictionary!";
	    helper.logEvent(event, 0, runInBackground, writelog);
	    event = "Operation Aborted!";
	    helper.logEvent(event, 0, runInBackground, writelog);
		exit(0);
	}
	event = "Dictionary Loaded Successfully...";
	helper.logEvent(event, 4, runInBackground, writelog);
	
	
	event = "Assigning Dictionary to BOW Descriptor Extractor...";
	helper.logEvent(event, 2, runInBackground, writelog);
	bowDE.setVocabulary(dictionary);
	event = "Dictionary Assigned...";
	helper.logEvent(event, 4, runInBackground, writelog);

	/*
	 * getHistAndLabels function in imgextract.cpp file called to extract
	 * histograms of each image in the training set (based on dictionary) and
	 * store them in a Mat and also to store of list of labels that are
	 * diesignated by file name (in this case) and store them in a Mat
	 */
	event = "Calculating BOW histograms...";
	helper.logEvent(event, 2, runInBackground, writelog);
	vector<string> files = vector<string>();	
	int totalEvalFiles = 0;
	string numFilesToProcess = "0";
	// if this is a whole directory process according
	if (isDir) {
		helper.GetFileList(evalDirectory, files);
		totalEvalFiles = files.size()-2;
		numFilesToProcess = static_cast<ostringstream*>( &(ostringstream() << (totalEvalFiles)) )->str();
		event = "Processing " + numFilesToProcess + " files in evaluation directory.";
		helper.logEvent(event, 2, runInBackground, writelog);
	} else {
		totalEvalFiles = 1;
		files.push_back(evalDirectory);	
		numFilesToProcess = "1";
		event = "Processing 1 file.";
		helper.logEvent(event, 2, runInBackground, writelog);
	}
	int numHistedFiles = 0;
	int wfcount = 0;
	string evalFileBase;
	for (unsigned int iz = 0;iz < files.size();iz++) {
		string fileToGetHist;
		if (isDir) {
			fileToGetHist = evalDirectory + files[iz];
			evalFileBase = files[iz];
		} else {
			fileToGetHist = evalDirectory;
			evalFileBase = evalDirectory; // need to fix this so that it splits the file path and extracts just the filename
		}
		event = "Processing file " + fileToGetHist + ".";
		helper.logEvent(event, 2, runInBackground, writelog);
		Mat fileHistogram =  getSingleImageHistogram(detector, bowDE, fileToGetHist);
		if (fileHistogram.rows > 0) {
			numHistedFiles++;
			event = fileToGetHist + " histogram calculated successfully.";
			helper.logEvent(event, 4, runInBackground, writelog);
			string histogramsCSV;
			string vwd = imagedoc.makeVWString(fileHistogram, true, 4, fileToGetHist, histogramsCSV);
			string filetoWrite = evalHistsDirectory;
			filetoWrite.append(evalFileBase);
			filetoWrite.append(".txt");
			int fileWriteRet = helper.writeTextFile(vwd, filetoWrite);
			if (fileWriteRet > 0) {
				event = "Visual Word Document for " + fileToGetHist + " written successfully to " + filetoWrite + ".";
				helper.logEvent(event, 4, runInBackground, writelog);
				wfcount++;
			} else {
				event = "Visual Word Document for " + fileToGetHist + " failed to write!";
				helper.logEvent(event, 0, runInBackground, writelog);
			}				
			
			
		} else {
			event = "Failed to calculate histogram for " + fileToGetHist;
			helper.logEvent(event, 0, runInBackground, writelog);
		}
	}
	
	string strNumHistedFiles = static_cast<ostringstream*>( &(ostringstream() << (numHistedFiles)) )->str();
	event = "Created histograms for " + strNumHistedFiles + " of " + numFilesToProcess + " files.";
	if (numHistedFiles < totalEvalFiles) {
		helper.logEvent(event, 0, runInBackground, writelog);
	} else {
		helper.logEvent(event, 4, runInBackground, writelog);		
	}
	
	string strWFcount = static_cast<ostringstream*>( &(ostringstream() << (numHistedFiles)) )->str();
	event = "Saved BOW files for indexing for " + strWFcount + " of " + numFilesToProcess + " files.";
	if (wfcount < totalEvalFiles) {
		helper.logEvent(event, 0, runInBackground, writelog);
	} else {
		helper.logEvent(event, 4, runInBackground, writelog);
	}

	event = "makeWordHistograms Process Completed.";
	helper.logEvent(event, 2, runInBackground, writelog);
	event = "Bye, Bye!";
	helper.logEvent(event, 2, runInBackground, writelog);

	return 0;

}