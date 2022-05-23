from flask import Flask, redirect, url_for, render_template, session, request
from flask_wtf import FlaskForm
from wtforms.fields import DateField, StringField, SelectField, TextAreaField, FileField, DateTimeField
from wtforms.validators import DataRequired
from wtforms import validators, SubmitField
from werkzeug.utils import secure_filename
import firebase_admin
from firebase_admin import db, credentials, initialize_app, storage
import pandas as pd
import datetime
from io import StringIO
import re
import os


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
sensors_ref = db.reference("/sensors")
scheduler_ref = db.reference("/scheduler")
bucket = storage.bucket()


class InfoForm(FlaskForm):
    startdate = DateField('', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    enddate = DateField('-', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    submit = SubmitField('Generate CSV')


sensors_data = sensors_ref.get()
sensors_ids = list(sensors_data.keys())


class editSensorForm(FlaskForm):
    sensor = SelectField(u'Sensor ID', choices=sensors_ids, validate_choice=True)
    image = FileField(u'Image [jpg]')
    location = StringField(u'Location')
    description = TextAreaField(u'Description')
    submit = SubmitField('Save')


class SchedulerForm(FlaskForm):
    startdate = StringField('', validators=(validators.DataRequired(),))
    enddate = StringField('', validators=(validators.DataRequired(),))
    submit = SubmitField('Add')


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


def arrangeQueryDate(date):
    date_strings = date.split("-")
    date = [int(d) for d in date_strings]
    date[2] += 2000 # because we save it as 22 we need to add 2000
    return date


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
        # print("[DEBUG] data: ", data)
        for key, val in data.items():
            if key == "call_id":
                continue
            key_arr = key.split(" ")
            date, time, sensor_id = key_arr
            date = arrangeQueryDate(date)
            print("[DEBUG] date: ", date)
            if not inRange(date, start_date, end_date):
                continue
            event = val['value']
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
    print("[DEBUG] real_data: ", real_data)
    curr_time = datetime.datetime.now()
    active_delta = datetime.timedelta(minutes=3)    # time that passed until asensor is inactive
    for sensor in active_sensors.keys():
        if sensor not in real_data:
            continue
        sensor_last_update_time = datetime.datetime.strptime(real_data[sensor]["time"], "%d-%m-%y %H:%M:%S")
        print("[DEBUG] sensor_last_update_time - curr_time: ", sensor_last_update_time - curr_time)
        if (sensor_last_update_time + active_delta >= curr_time):
            if real_data[sensor]["value"] == "ERROR":
                active_sensors[sensor] = 2
            else:
                active_sensors[sensor] = 1
            print("[DEBUG]Sensor: '", sensor, "' is active")
        else:
            print("[DEBUG] Sensor: '", sensor, "' is not active")
            active_sensors[sensor] = 0
    # pass the list to the return
    # loop through it in the html with JINGA and update the active sensors
    return render_template("status.html", active_sensors=active_sensors)


@app.route("/sensors",  methods=['GET','POST'])
def getSensors():
    # get the sensors data from the firebase
    form = editSensorForm()
    sensors_data = sensors_ref.get()
    sensors_locations = [tmp_d['location'] for tmp_d in list(sensors_data.values())]
    sensors_descriptions = [tmp_d['description'] for tmp_d in list(sensors_data.values())]
    sensor_image_prefixes = [tmp_d['image'] for tmp_d in list(sensors_data.values())]
    if form.validate_on_submit():
        sensor_id = form.sensor.data
        sensor_image_prefix = sensors_data[sensor_id]['image']
        if form.image.data:
            uploaded_file = request.files[form.image.name]
            print(f"uploaded_file:  {uploaded_file}")
            uploaded_file.save('images/'+sensor_id + ".jpg")
            blob = bucket.blob('images/'+sensor_id+str(sensor_image_prefix)+'.jpg')
            with open('images/'+sensor_id + '.jpg', 'rb') as img:
                # blob.delete()
                sensor_image_prefix += 1
                blob = bucket.blob('images/'+sensor_id+'_'+str(sensor_image_prefix)+'.jpg')
                print('images/'+sensor_id+'_'+str(sensor_image_prefix)+'.jpg')
                blob.upload_from_file(img, content_type='image/jpeg')
                blob.make_public()
            print(blob.public_url)
        s_location = form.location.data if form.location.data else sensors_data[sensor_id]['location']
        s_description = form.description.data if form.description.data else sensors_data[sensor_id]['description']
        new_data = {sensor_id: {'location':s_location, 'description':s_description, 'image':sensor_image_prefix}}
        sensors_ref.update(new_data)
        return redirect('sensors')
    return render_template("sensors.html", s_ids=sensors_ids, s_locations=sensors_locations, s_descriptions=sensors_descriptions,
                            sensor_image_prefixes=sensor_image_prefixes, form=form)


@app.route("/scheduler", methods=['GET','POST'])
def scheduler():
    form = SchedulerForm()
    scheduler_data = scheduler_ref.get()
    parsed_data = []
    for i in range(len(scheduler_data)):
        if scheduler_data[i]:
            for s,e in scheduler_data[i].items():
                parsed_data.append((i, s, e))
    res = session['res'] if 'res' in session else ''
    if res:
        session.pop('res')
    print(f"validate_on_submit: {form.validate_on_submit()}")
    print(f"startdate: {form.startdate.data}, enddate: {form.enddate.data}")
    if form.validate_on_submit():
        # validate date is ok within the scheduler_data
        session['startdate'] = form.startdate.data
        session['enddate'] = form.enddate.data
        print(f"startdate: {form.startdate.data}, enddate: {form.enddate.data}")
        return redirect('addSchedule')
    return render_template("scheduler.html", scheduler_data=parsed_data, form=form, res=res)


@app.route("/deleteSchedule")
def deleteSchedule():
    schedule_id = request.args.get('id')
    schedule_to_delete = db.reference(f'scheduler/{schedule_id}')
    schedule_to_delete.delete()
    return redirect('scheduler')


@app.route("/addSchedule")
def addSchedule():
    startdate = session['startdate']
    enddate = session['enddate']
    # res = generateCSV(startdate, enddate)
    # if res == "ERROR":
    #     session['res'] = 'Please enter a valid date range'
    #     return redirect(url_for("home", res='Please enter a valid date range'))
    # else:
    #     session['res'] = res
    #     return redirect(url_for("scheduler", res=res))
    print(f"startdate: {startdate}, enddate: {enddate}")
    return redirect(url_for("scheduler"))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=1234)