var express = require("express");
process.env.NODE_ENV = process.env.NODE_ENV || 'development';
require('dotenv').config({path: './config/.' + process.env.NODE_ENV})
var request = require("request");
var bodyParser = require("body-parser");
var app     = express();
var path    = require("path");
var fs = require('fs');
// var checkUrl = 'http://ec2-34-194-141-56.compute-1.amazonaws.com:50050/';
// var idServiceUrl = 'http://ec2-34-194-141-56.compute-1.amazonaws.com:50060/';
var AWS = require('aws-sdk');

var s3 = new AWS.S3({
	accessKeyId: process.env.S3ACCESSKEYID,
	secretAccessKey: process.env.S3SECRETACCESSKEY,
	signatureVersion: 'v4'
})

function requireHTTPS(req, res, next) {
    if (!req.secure && req.get('X-Forwarded-Proto') != 'https') {
        return res.redirect('https://' + req.hostname + req.url);
    }
    return next();
}

function isSafari(userAgent) {
	// var ua = userAgent && userAgent.toLowerCase();
	// if (ua.indexOf('safari') != -1) { 
	// 	if (ua.indexOf('chrome') > -1) {
	// 		return false; // Chrome
	// 	} else {
	// 		return true; // Safari
	// 	}
	// }
	return false;
}

//app.use(requireHTTPS);
app.use(express.static('public'));
app.use(bodyParser({limit: '50mb'}));
app.use(bodyParser.json()); // support json encoded bodies
app.use(bodyParser.urlencoded({ extended: true }));

app.get('/',function(req,res){
	if(isSafari(req.headers['user-agent'])) {
		return res.send('Please open this page in chrome or firefox');
	}
	return res.sendFile(path.join(__dirname + '/index.html'));
	console.log('GOT REAUEST ON /')
});

app.get('/blinks', function(req, res) {
	res.sendFile(path.join(__dirname + '/blinks.html'))
})

app.get('/objects', function(req, res) {
	res.sendFile(path.join(__dirname + '/objects.html'));
});

app.post('/objects/check', function(req, res) {
	var matches = req.body.image.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	fs.writeFile("./objimages/capture.jpg", new Buffer(matches[2], "base64"), function(err) {
		var formData = {
			'image-rec': fs.createReadStream('./objimages/capture.jpg')
		};
		request.post({url: process.env.OBJECTDETECTIONURL, headers: {'Content-Type': 'multipart/form-data'}, formData: formData}, function callback(err, httpResponse, body) {
			console.log(body)
			var result = 'Other';
			if (!err && httpResponse.statusCode == 200) {
				var responseObject = JSON.parse(body);
				var max = -10000;
				Object.keys(responseObject).forEach(function(key) {
					var tmp = parseFloat(responseObject[key]);
					if (tmp > max) {
						max = tmp;
						result = key;
					}
				});
			}
			if (result === 'background')
				result = "Other";
			res.send(result)
		});
		
	});
});

app.get('/ocr',function(req,res){
	res.sendFile(path.join(__dirname + '/recognition.html'));
});

app.get('/doc', function(req,res) {
	res.sendFile(path.join(__dirname + '/doccheck.html'));
})

app.get('/docface', function(req, res) {
	res.sendFile(path.join(__dirname + '/doccheck2.html'));
})

var saveFileToS3 = (path, imageName, data) => {
		var params = {
			Bucket: process.env.S3BUCKET,
			Key: path + imageName,
			Body: data
		}
	
		s3.putObject(params, function(error, data) {
			if (error)
				console.log(error, error.stack);
		});
}

app.post('/upload', function(req,res) {

	var matches = req.body.image.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var imageBuffer = new Buffer(matches[2], "base64");
	var timestamp = Date.now();
	//fs.writeFile("./images/aa.jpg", new Buffer(req.body.image, "base64"), function(err) {});
	// fs.writeFile("./images/capture.jpg", new Buffer(matches[2], "base64"), function(err) {
	// 	console.log('sending data...');
	// 	var formData = {
	// 		person_image: fs.createReadStream('./images/capture.jpg'),
	// 		image1: fs.createReadStream('./images/Zaal Gachechiladze.jpg'),
	// 		image2: fs.createReadStream('./images/Dachi Choladze.jpg'),
	// 		image3: fs.createReadStream('./images/Amiran Sherozia.jpg'),
	// 		image4: fs.createReadStream('./images/Bidzina Matsaberidze.jpg'),
	// 		image5: fs.createReadStream('./images/Giorgi Mezurnishvili.jpg'),
	// 		image6: fs.createReadStream('./images/Ketevan Bosikashvili.jpg'),
	// 		image7: fs.createReadStream('./images/Vazha Beriashvili.jpg'),
	// 		image8: fs.createReadStream('./images/Zviad Tsotskolauri.jpg'),
	// 		image9: fs.createReadStream('./images/Giorgi Peikrishvili.jpg'),
	// 		image10: fs.createReadStream('./images/Irakli Davarashvili.jpg'),
	// 		image11: fs.createReadStream('./images/Ketevan Bosikashvili.jpg'),
	// 		image12: fs.createReadStream('./images/Vazha Beriashvili.jpg'),
	// 		image13: fs.createReadStream('./images/Dimitri Mikadze.jpg'),
	// 		image14: fs.createReadStream('./images/Goga Gujejiani.jpg'),
	// 		image15: fs.createReadStream('./images/Levan Namgaladze.jpg'),
	// 		image16: fs.createReadStream('./images/Nikoloz Parkosadze.jpg'),
	// 		image17: fs.createReadStream('./images/Sandro Maghlakelidze.jpg'),
	// 		image19: fs.createReadStream('./images/Beka Tomashvili.jpg'),
	// 		image20: fs.createReadStream('./images/Nia Odzelashvili.jpg'),
	// 		image21: fs.createReadStream('./images/Nikoloz Kurdiani.jpg'),
	// 		image22: fs.createReadStream('./images/Nino Masurashvili.jpg'),
	// 		image23: fs.createReadStream('./images/Gabriel Meliva.jpg'),
	// 		image24: fs.createReadStream('./images/Gabo Meliva.jpg'),
	// 		image24: fs.createReadStream('./images/Sandro Kiknadze.jpg'),
	// 		image25: fs.createReadStream('./images/Vakhtang Butskhrikidze.jpg'),
	// 		image26: fs.createReadStream('./images/Eka Kvirikashvili.jpg'),
	// 		image27: fs.createReadStream('./images/Nino Gujejiani.jpg'),
	// 		image28: fs.createReadStream('./images/Irakli Urushadze.jpg'),
	// 		image29: fs.createReadStream('./images/Archil Talakvadze.png'),
	// 		image30: fs.createReadStream('./images/Archil_Talakvadze.png'),
	// 		image31: fs.createReadStream('./images/Archil-Talakvadze.png'),
	// 		image32: fs.createReadStream('./images/Kakha Kaladze.jpeg'),
	// 		image33: fs.createReadStream('./images/Kakhaber Kaladze.png'),
	// 		image34: fs.createReadStream('./images/Kakhi Kaladze.png'),
	// 		image35: fs.createReadStream('./images/Levan Abashidze.jpeg'),
	// 		image36: fs.createReadStream('./images/Tea Kobakhidze.jpg'),
	// 		image37: fs.createReadStream('./images/Giorgi Vakhtangishvili.jpg'),
	// 		image38: fs.createReadStream('./images/Koka Kamushadze.jpeg'),
	// 		image39: fs.createReadStream('./images/Nika Lachkepiani.jpeg'),
	// 		image40: fs.createReadStream('./images/Nika_Lachkepiani.png'),
	// 		image41: fs.createReadStream('./images/Khatuna Abashidze.jpeg'),
	// 		image42: fs.createReadStream('./images/Giorgi Maisuradze.jpeg'),
	// 		image43: fs.createReadStream('./images/Akaki Pertaia.jpeg'),
	// 		image44: fs.createReadStream('./images/Manana Chinchaladze.jpg'),
	// 		image45: fs.createReadStream('./images/Tamar Pertia.jpg'),
	// 		image46: fs.createReadStream('./images/Irakli Papava.jpg'),
	// 		image47: fs.createReadStream('./images/Revaz Papukashvili.jpg'),
	// 		image48: fs.createReadStream('./images/Givi Murvanidze.jpg'),
	// 		image49: fs.createReadStream('./images/Nodar Kakriashvili.jpg'),
	// 		image50: fs.createReadStream('./images/Sopo Chkoidze.jpeg')			
	// 	}
	// 	//console.log(formData);
	// 	request.post({url: process.env.CHECKURL, headers: {'Content-Type': 'multipart/form-data'}, formData: formData}, function callback(err, httpResponse, body) {
	// 		console.log(err, httpResponse)
	// 		console.log(body);
	// 		if (!err && httpResponse.statusCode == 200) {
	// 			var responseObject = JSON.parse(body);
	// 			var responseText = 'Could Not Identify';
	// 			if (responseObject.hasOwnProperty('status') && responseObject.status.code == -1) {
	// 				res.send(responseText);
	// 				return;
	// 			}
	// 			var minScore = 10000;
	// 			for(var item in responseObject) {
	// 				if (responseObject[item].comp_result[0][1] == true &&  minScore > parseFloat(responseObject[item].comp_result[0][0])) {
	// 					minScore = parseFloat(responseObject[item].comp_result[0]);
	// 					responseText = responseObject[item].fileName.replace(/\.[^/.]+$/, "");
	// 				}
	// 			}
	// 			saveFileToS3(process.env.S3PATHFACEID, responseText + timestamp + '.jpg', imageBuffer);
	// 			res.send(responseText);
	// 		} else
	// 		res.send('Could Not Identify');
	// 	});
	// });
	
});

var checkFields = (response) => {
	var count = 0;
	Object.keys(response).forEach((item) => {
		if (item !== 'document_type' && response[item])
			count++;
	})
	return count > 1
}

app.post('/docface/check', function(req, res) {
	var selfie = req.body.selfie.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var idfront = req.body.idfront.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var selfieBuffer = new Buffer(selfie[2], "base64");
	var idFrontBuffer = new Buffer(idfront[2], "base64");
	var timestamp = Date.now();
	saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'selfie.jpg', selfieBuffer);
	saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'idfront.jpg', idFrontBuffer);
	var checkTime;
	fs.writeFileSync('./docimages/selfie.jpg', selfieBuffer, {flags: 'w+'});
	fs.writeFileSync('./docimages/idfront.jpg', idFrontBuffer, {flags: 'w+'});
	formData = {
		person_image: fs.createReadStream('./docimages/selfie.jpg'),
		image1: fs.createReadStream('./docimages/idfront.jpg')
	};
	checkData = {
		file: fs.createReadStream('./docimages/idfront.jpg')
	};
	checkTime = Date.now();
	request.post({url: process.env.CHECKURL, headers: {'Content-Type': 'multipart/form-data'}, formData: formData}, function callback(err, httpResponse, body) {
		console.log(err)
		console.log(body);
		if (!err && httpResponse.statusCode == 200) {
			var responseObject = JSON.parse(body);
			var responseText = 'NO';
			if (responseObject.hasOwnProperty('status') && responseObject.status.code == -1) {
				saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: ' + responseText + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
				res.send(responseText);
				return;
			}
			var minScore = 10000;
			for(var item in responseObject) {
				if (!responseObject[item].comp_result.length)
					continue;
				if (responseObject[item].comp_result[0][1] == true &&  minScore > parseFloat(responseObject[item].comp_result[0][0])) {
					minScore = parseFloat(responseObject[item].comp_result[0]);
					responseText = 'YES';
				}
			}
			saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: ' + responseText + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
			res.send(responseText);
		} else {
			saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: No ID Card Detected' + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
			res.send('No ID Card Detected');
		}
	});
})

app.post('/doc/check', function(req, res) {
	var selfie = req.body.selfie.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var idfront = req.body.idfront.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var selfieBuffer = new Buffer(selfie[2], "base64");
	var idFrontBuffer = new Buffer(idfront[2], "base64");
	var timestamp = Date.now();
	saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'selfie.jpg', selfieBuffer);
	saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'idfront.jpg', idFrontBuffer);
	var checkTime;
	fs.writeFileSync('./docimages/selfie.jpg', selfieBuffer, {flags: 'w+'});
	fs.writeFileSync('./docimages/idfront.jpg', idFrontBuffer, {flags: 'w+'});
	formData = {
		person_image: fs.createReadStream('./docimages/selfie.jpg'),
		image1: fs.createReadStream('./docimages/idfront.jpg')
	};
	checkData = {
		file: fs.createReadStream('./docimages/idfront.jpg')
	};
	checkTime = Date.now();
	request.post({url: process.env.DOCUMENTCHECKURL, headers: {'Content-Type': 'multipart/form-data'}, formData: checkData}, function cb(error, httpRes, bd) {
		console.log(error);
		console.log(bd);
		if (!error && httpRes.statusCode == 200) {
			var obj = JSON.parse(bd);
			if ( (obj.document_type) && (obj.document_type !== '') && checkFields(obj) ) {
				request.post({url: process.env.CHECKURL, headers: {'Content-Type': 'multipart/form-data'}, formData: formData}, function callback(err, httpResponse, body) {
					console.log(err)
					console.log(body);
					if (!err && httpResponse.statusCode == 200) {
						var responseObject = JSON.parse(body);
						var responseText = 'NO';
						if (responseObject.hasOwnProperty('status') && responseObject.status.code == -1) {
							saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: ' + responseText + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
							res.send(responseText);
							return;
						}
						var minScore = 10000;
						for(var item in responseObject) {
							if (!responseObject[item].comp_result.length)
								continue;
							if (responseObject[item].comp_result[0][1] == true &&  minScore > parseFloat(responseObject[item].comp_result[0][0])) {
								minScore = parseFloat(responseObject[item].comp_result[0]);
								responseText = 'YES';
							}
						}
						saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: ' + responseText + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
						res.send(responseText);
					} else {
						saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: No ID Card Detected' + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
						res.send('No ID Card Detected');
					}
				});
			} else {
				saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: No ID Card Detected' + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
				res.send('No ID Card Detected');
			}
		} else {
			saveFileToS3(process.env.S3PATHDOC + timestamp + '/', 'response.txt', 'Check Result: No ID Card Detected' + '\nResponse time: ' + (Date.now() - checkTime) + 'ms');
			res.send('No ID Card Detected');
		}
	});
})

app.post('/ocr/check', function(req,res) {
	var selfie = req.body.selfie.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var idfront = req.body.idfront.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var idback = req.body.idback.match(/^data:([A-Za-z-+\/]+);base64,(.+)$/);
	var timestamp = Date.now();
	var selfieBuffer = new Buffer(selfie[2], "base64");
	var idFrontBuffer = new Buffer(idfront[2], "base64");
	var idBackBuffer = new Buffer(idback[2], "base64");
	//save images to s3
	saveFileToS3(process.env.S3PATHOCR, 'selfie' + timestamp + '.jpg', selfieBuffer);
	saveFileToS3(process.env.S3PATHOCR, 'idfront' +timestamp + '.jpg', idFrontBuffer);
	saveFileToS3(process.env.S3PATHOCR, 'idback' +timestamp + '.jpg', idBackBuffer);

	//store files temporarily
	fs.writeFileSync('./ocrimages/selfie.jpg', selfieBuffer, {flags: 'w+'});
	fs.writeFileSync('./ocrimages/idfront.jpg', idFrontBuffer, {flags: 'w+'});
	fs.writeFileSync('./ocrimages/idback.jpg', idBackBuffer, {flags: 'w+'});

	console.log('images saved');

	//check person
	formData = {
		person_image: fs.createReadStream('./ocrimages/selfie.jpg'),
		image1: fs.createReadStream('./ocrimages/idfront.jpg')
	}
	var result = {};
	request.post({url: process.env.CHECKURL, headers: {'Content-Type': 'multipart/form-data'}, formData: formData}, function callback(err, httpResponse, body) {
		console.log(':::::::::::::check person response::::::::::::::')
		console.log(body);
		result.responseText = './img/notidentified.png';
		if (!err && httpResponse.statusCode == 200) {
			var responseObject = JSON.parse(body);
				for(var item in responseObject) {
					if (responseObject[item].comp_result[0][1] == true) {
						result.responseText = './img/identified.png';
						break;
					}
				}
		}
		//get id card fields
		idFormData = {
			id_card: fs.createReadStream('./ocrimages/idback.jpg')
		}
		request.post({url: process.env.OCRURL, headers: {'Content-Type': 'multipart/form-data'}, formData: idFormData}, function callback(err1, httpResponse1, body1) {
			console.log(':::::::::::::::OCR resonse:::::::::::::::::')
			console.log(body1);
			result.firstName = '----';
			result.lastName = '----';
			result.birthDate = '----';
			result.personId = '----';
			result.documentId = '-----';
			if (!err1 && httpResponse1.statusCode == 200) {
				var responseObj = JSON.parse(body1);
				console.log(responseObj);
				if (responseObj.status == 'ok') {
					result.firstName = responseObj.personInfo.firstName;
					result.lastName = responseObj.personInfo.lastName;
					result.birthDate = responseObj.personInfo.birthDate;
					result.personId = responseObj.personInfo.personId;
					result.documentId = responseObj.personInfo.documentId;
				}
			}
			res.send(JSON.stringify(result));
		});
	});
});

app.listen(process.env.PORT);

console.log("Running on Port " + process.env.PORT);