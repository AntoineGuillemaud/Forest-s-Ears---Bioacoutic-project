from numpy.fft import fft
from scipy.io import wavfile
import matplotlib.pyplot as plt
import time
import numpy as np
import json
import os
import datetime
import tensorflow as tf

def extract_header(filename,ip):
    #extract timestamp
    year=2022
    month=filename[11:13]
    day=filename[7:9]
    hour=filename[14:16]
    minute=filename[17:19]
    date_string=str(year)+"/"+str(month)+"/"+str(day)+"/"+str(hour)+"/"+str(minute)
    date = datetime.datetime.strptime(date_string, "%Y/%m/%d/%H/%M")
    timestamp = datetime.datetime.timestamp(date)

    #extract id
    id=0
    file_path="./ID"+str(ip)+"/"+filename
    file=open(file_path,"rb")
    header=file.read(48)
    id_char=int(header[40])
    if id_char==31:
        id=1
    elif id_char==47:
        id=2
    elif id_char==63:
        id=3
    elif id_char==79:
        id=4
    file.close()
    return timestamp,id

def stereo_to_mono(samples):
    samples = samples.sum(axis=1) / 2
    return samples.astype(int)

def spectrogram(sample_rate,samples,width,height):
    result = np.zeros((width,height))
    for i in range(width):
        a = np.absolute(fft(samples[int(i*(samples.size/width)):int((i+1)*(samples.size/width))]))
        a = np.resize(np.mean(np.resize((a[0:int(a.size/2)]),(height,int(len(a)/(height*2)))),axis = 1),height)
        result[i] = a
    return np.rot90(result,1)

new_model = tf.keras.models.load_model('save_encoder.h5')

def compute_middle_dim(spectrogrammm):
    b=np.array([spectrogrammm])

    #a=list(np.array(new_model(b)))
    a=new_model(b)
    a=a.numpy()
    a=a.tolist()[0]
    return a



if __name__== "__main__" :
    print("[info] : début du calcul des spectrogrammes")

    sample_duration = 2.5 # taille de l'image en seconde
    xlen = 32
    ylen =32
    step_print = 100

    files = [os.listdir("./ID1"), os.listdir("./ID2") ,  os.listdir("./ID3") ,  os.listdir("./ID4") ]
    
    ip=0
    for id_folder in files :
        ip+=1
        for file in id_folder:



            if file[-4:] == ".wav" :
                result = []

                sample_rate ,samples = wavfile.read("./ID"+str(ip)+"/"+file)
                print("[info] : lecture de ",file)

                if samples[0].size > 1:
                    samples = stereo_to_mono(samples)

                print("[info] : debut du calcul des spectrogrammes")
                t = time.time()
                for i in range(int(len(samples)/(sample_duration*sample_rate))):
                    timestamp, id =  extract_header(file,ip)
                    spectrogramme =spectrogram(sample_rate,np.array(samples[int(i*sample_rate*sample_duration):int((i+1)*sample_rate*sample_duration)]),width = xlen, height = ylen)
                    
                    spectrogrammm = (256*(spectrogramme/np.amax(spectrogramme ))).astype(int).tolist()
                    data = {"timestamp" : int(timestamp+i*sample_duration),
                            "id" : id,
                            "duration" : sample_duration,
                            "spectrogramme" : spectrogrammm ,
                            "middle_dim" : compute_middle_dim(spectrogrammm)
                         }





                    result.append(data)



                        #plt.imshow(result[-1], interpolation='antialiased',cmap = 'PuOr')
                        #plt.show()
                Myfile = open("./ID"+str(ip)+"/true_samples/"+file[:-4]+".json","w")
                json.dump(result,Myfile,indent = 3)
                Myfile.close()
                print("[info] : {nb} spectrogram calculés en {time}s.".format(time = time.time()-t, nb= int(len(samples)/(sample_duration*sample_rate))))
