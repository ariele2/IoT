from flask import Flask, redirect, url_for, render_template, session
from flask_wtf import FlaskForm
from wtforms.fields import DateField
from wtforms.validators import DataRequired
from wtforms import validators, SubmitField
import firebase_admin
from firebase_admin import db, credentials, initialize_app, storage
import pandas as pd
import datetime


app = Flask(__name__)

app.config['SECRET_KEY'] = '#$%^&*'

cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
storage_path = 'iotprojdb.appspot.com'
firebase_path = 'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'
default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':firebase_path, 'storageBucket': storage_path})

csv_path = "csv_reports/"
action_ref = db.reference("/action")
data_ref = db.reference("/data")
real_data_ref = db.reference("/real_data")
bucket = storage.bucket()


class InfoForm(FlaskForm):
    startdate = DateField('', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    enddate = DateField('-', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    submit = SubmitField('Generate CSV')


def updateDB():
    action_data = action_ref.get()
    if action_data == "off":
        action_ref.set("on")
        return "on"
    action_ref.set("off")
    return "off"


def date2num(date):
    list_date = date.split(' ')
    day = int(list_date[1])
    month = datetime.datetime.strptime(list_date[2], "%b").month
    year = int(list_date[3])
    return [day,month,year]


def validateDates(start_date, end_date):
    if end_date[2] - start_date[2] < 0:     # end year is smaller then start year
        return False
    elif end_date[2] - start_date[2] > 0:
        return True
    else:   # the same year
        if end_date[1] - start_date[1] < 0:
            return False
        elif end_date[1] - start_date[1] > 0:
            return True
        else:   # the same month
            if end_date[0] - start_date[0] < 0:
                return False
            else:   # the same day or greater
                return True
    return False    # shouldn't reach here


def inRange(date, start_date, end_date):
    # print("[DEBUG] date: ", date, ", start_date: ", start_date, ", end_date: ", end_date)
    if date[2] > end_date[2] or date[2] < start_date[2]:    # data's year is not in the range 
        return False
    # same year for start, end and date, so need to check months, otherwise it means there is a legal year differential
    elif date[2] == end_date[2] and date[2] == start_date[2]:   
        if date[1] > end_date[1] or date[1] < start_date[1]:
            return False
        # same month for start, end and date, so need to check days, otherwise it means there is a legal month differntial.
        elif date[1] == end_date[1] and date[1] == start_date[1]:  
            if date[0] > end_date[0] or date[0] < start_date[0]:
                return False
    # if reached here, the date we found is good to go inside the .csv file!
    return True 


def generateCSV(start_date, end_date):
    # create the columns titles
    columns_titles = ["sensorID", "Type", "Event", "Time", "Day"]
    df = pd.DataFrame(columns=columns_titles)
    # transfer date value to readable integers
    start_date = date2num(start_date)
    end_date = date2num(end_date)
    # validate the dates
    if not validateDates(start_date, end_date):
        return "ERROR"
    # query the dates and arrange in the dataframe
    data = data_ref.get()
    if data:
        for key, val in data.items():
            key_arr = key.split(" ")
            date, time, sensor_id = key_arr
            print("[DEBUG] date: ", date)
            if not inRange(date, start_date, end_date):
                continue
            event = val['Value']
            day = str(date[0]) + '/' + str(date[1]) + '/' + str(date[2])
            # print("[DEBUG] sensor_id: ", sensor_id, ", event: ", event, "time: ", time, ", day: ", day)
            df.loc[len(df.index)] = [sensor_id, "-", event, time, day]
    # create the csv file
    start_date_str = str(start_date[0]) + '_' + str(start_date[1]) + '_' + str(start_date[2])
    end_date_str = str(end_date[0]) + '_' + str(end_date[1]) + '_' + str(end_date[2])
    new_csv_filename = csv_path + "report_" + start_date_str + "-" + end_date_str + ".csv"
    df.to_csv(new_csv_filename, index=False)
    # upload the file to the storage
    blob = bucket.blob(new_csv_filename)
    with open(new_csv_filename, 'rb') as f:
        blob.upload_from_file(f)
    blob.make_public()
    # generate a download url and return it
    return blob.public_url


@app.route("/", methods=['GET','POST'])  # this sets the route to this page
def home():
    form = InfoForm()
    action_data = action_ref.get()
    res = session['res'] if 'res' in session else ''
    if res == 'Please enter a valid date range':
        session.pop('res')
        res = 'Please enter a valid date range'
    if form.validate_on_submit():
        session['startdate'] = form.startdate.data
        session['enddate'] = form.enddate.data
        return redirect('generateCSV')
    return render_template("index.html", val=action_data, form=form, res=res)


@app.route("/updateDB")
def update_db():
    res = updateDB()
    return redirect(url_for("home"))


@app.route("/generateCSV")
def generate_csv():
    startdate = session['startdate']
    enddate = session['enddate']
    err_message = 'Please enter a valid date range'
    res = generateCSV(startdate, enddate)
    if res == "ERROR":
        session['res'] = err_message
        return redirect(url_for("home", res=err_message))
    else:
        session['res'] = res
        return redirect(url_for("home", res=res))


@app.route("/status")
def getStatus():
    # make a dict of all of the sensors (for now hard coded list) as keys, val initialized to 0
    active_sensors = {'S-01':0, 'S-02':0, 'S-03':0,'S-04':0,'S-05':0,'S-06':0,'S-07':0,'S-08':0,'S-09':0,'S-10':0, 'D-01':0, 'V-01':0}
    # query the real_data from the realtime db
    real_data = real_data_ref.get()
    curr_time = datetime.datetime.now()
    active_delta = datetime.timedelta(minutes=3)    # time that passed until asensor is inactive
    for sensor in active_sensors.keys():
        if sensor not in real_data:
            continue
        sensor_last_update_time = datetime.datetime.strptime(real_data[sensor]["time"], "%d-%m-%y %H:%M:%S")
        if (sensor_last_update_time + active_delta >= curr_time):
            if real_data[sensor]["value"] == "ERROR":
                active_sensors[sensor] = 2
            else:
                active_sensors[sensor] = 1
        else:
            active_sensors[sensor] = 0
    # pass the list to the return
    # loop through it in the html with JINGA and update the active sensors
    return render_template("status.html", active_sensors=active_sensors)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=1234)