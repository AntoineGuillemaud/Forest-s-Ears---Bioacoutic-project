from tensorflow.keras.models import Model
from tensorflow.keras import layers, losses
import tensorflow as tf


import matplotlib.pyplot as plt

import numpy as np
import os
import json

latent_dim = 64


load_data_ratio=25 #en pourcent

test=1
if test==0:
    dataset = tf.keras.datasets.fashion_mnist
    (x_train, y_train),(x_test, y_test) = dataset.load_data()
    x_train, x_test = x_train / 255.0, x_test / 255.0
elif test==1:
    ## Loading the dataset and then normalizing the images.
    def get_datas(path,id,is_train):
        l = 0
        data = []
        files = os.listdir(path)
        np.random.shuffle(files)
        for file in files[:int((len(files)*load_data_ratio)/100)] :
            if (is_train and file not in test_file_list[id-1]) or (not is_train and file in test_file_list[id-1]):
                print("chargement de : ",file)
                if file[-5:] == ".json" :
                    a=1
                    MyFile = open(path+ "/" + file,"r")
                    file = json.load(MyFile)
                    for sample in file : 
                        data.append(sample["spectrogramme"])
                    MyFile.close()
        print("[info] : donnée(s) chargées avec succès")
        return data[1:]
    
    test_file_list=[]   
    test_file_list_file=open("test_file_list.json","r")
    test_file_list = json.load(test_file_list_file)
    
    train_datas = get_datas("./ID1/true_samples",1,True) + get_datas("./ID2/true_samples",2,True) + get_datas("./ID3/true_samples",3,True) + get_datas("./ID4/true_samples",4,True)
    test_datas = get_datas("./ID1/true_samples",1,False) + get_datas("./ID2/true_samples",2,False) + get_datas("./ID3/true_samples",3,False) + get_datas("./ID4/true_samples",4,False)
    print(len(test_datas))
    max1 = np.amax(train_datas)
    max2 = np.amax(test_datas)
    max=max(max1,max2)
    train_datas = (train_datas/max)*255
    test_datas = (test_datas/max)*255
    x_train = train_datas.astype("float32")/255.
    x_test = test_datas.astype("float32")/255.
    np.random.shuffle(x_test)
elif test==2:
    Myfilej = open("./json_data/ALL_AVRIL_MAI.json","r")
    datas=json.load(MyFilej)
    max = np.amax(datas)
    datas = (datas/max)*255
    x_train = datas[int(len(datas)*0.8):].astype("float32")/255.
    x_test = datas[:int(len(datas)*0.8)].astype("float32")/255.
    np.random.shuffle(x_test)

#creating save folder
try: 
    os.mkdir("saveModel2")
except:
    pass
    
i=1
save_path= "saveModel2/save"+str(i)
while(os.path.isfile(save_path)):
    i+=1
    save_path= "saveModel2/save"+str(i)
try: 
    os.mkdir(save_path)
except:
    pass



class Autoencoder(Model):
    def __init__(self,latent_dim,size):
        print("[info] : création d'une instance de classe Autoencoder")
        super(Autoencoder,self).__init__()
        self.latent_dim = latent_dim

        #encoder
        self.encoder = tf.keras.Sequential([
        layers.Flatten(),
        layers.Dense(latent_dim*2, activation = "relu"),
        layers.Dense(latent_dim*2,activation = "relu"),
        layers.Dense(latent_dim, activation = "relu")
        ])

        #decoder
        self.decoder = tf.keras.Sequential([
        layers.Dense(latent_dim, activation = "relu"),
        layers.Dense(latent_dim*2,activation = "relu"),
        layers.Dense(size[1]*size[0], activation='sigmoid'),
        layers.Reshape(size)
        ])

    def call(self,x):
        encoded = self.encoder(x)
        decoded = self.decoder(encoded)
        return decoded

    def __del__(self):
        print("[info] : destruction d'une instance de classe Autoencoder")



autoencoder = Autoencoder(latent_dim,(32,32))
autoencoder.compile(optimizer = "adam",loss =losses.MeanSquaredError() )

backup_dir = save_path + "/backup"
callback = tf.keras.callbacks.BackupAndRestore(backup_dir)

autoencoder.fit(x_train, x_train,epochs=20,shuffle=True,validation_data=(x_test, x_test),callbacks=[callback])
encoded_imgs = autoencoder.encoder(x_test).numpy()
decoded_imgs = autoencoder.decoder(encoded_imgs).numpy()


autoencoder.save(save_path + "/save_dossier")
autoencoder.save(save_path + "/save_ficher.h5")

n = 10
plt.figure(figsize=(20, 4))
for i in range(n):
  # display original
  ax = plt.subplot(2, n, i + 1)
  plt.imshow(x_test[i])
  plt.title("original")
  plt.gray()
  ax.get_xaxis().set_visible(False)
  ax.get_yaxis().set_visible(False)

  # display reconstruction
  ax = plt.subplot(2, n, i + 1 + n)
  plt.imshow(decoded_imgs[i])
  plt.title("reconstructed")
  plt.gray()
  ax.get_xaxis().set_visible(False)
  ax.get_yaxis().set_visible(False)
plt.savefig(save_path + "/autoencoder1.png")
plt.show()
