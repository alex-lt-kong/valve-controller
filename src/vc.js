import * as React from 'react';
import ReactDOM from 'react-dom';
import axios from 'axios';
import Grid from '@mui/material/Grid'; // Grid version 1
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import Card from '@mui/material/Card';
import Button from '@mui/material/Button';
import CardActions from '@mui/material/CardActions';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Select from '@mui/material/Select';
import MenuItem from '@mui/material/MenuItem';
import InputLabel from '@mui/material/InputLabel';
import GasMeterIcon from '@mui/icons-material/GasMeter';
import FormControl from '@mui/material/FormControl';
import Stack from '@mui/material/Stack';
import LinearProgress from '@mui/material/LinearProgress';
import PropTypes from 'prop-types';


function LinearProgressWithLabel(props) {
  return (
    <Box sx={{display: 'flex', alignItems: 'center'}} mx='2em'>
      <Box sx={{width: '100%', mr: 1}}>
        <LinearProgress variant="determinate" {...props} />
      </Box>
      <Box sx={{minWidth: '4em'}}>
        <Typography variant="body2" color="text.secondary">
          {`${props.secondstotal - props.seconds}/${props.secondstotal}秒`}
        </Typography>
      </Box>
    </Box>
  );
}

LinearProgressWithLabel.propTypes = {
  /**
   * The value of the progress indicator for the determinate and buffer variants.
   * Value between 0 and 100.
   */
  value: PropTypes.number.isRequired,
  secondstotal: PropTypes.number,
  seconds: PropTypes.number
};


class LiveImages extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      imageEndpoint: './get_live_image_jpg/',
      valveSessionLengthSec: 10,
      simpleSnackbar: null,
      secondsTotal: 0,
      seconds: 0
    };
    this.timer = 0;
    this.startTimer = this.startTimer.bind(this);
    this.countDown = this.countDown.bind(this);

    this.onSelectItemChange = this.onSelectItemChange.bind(this);
    this.onOpenValveButtonClick = this.onOpenValveButtonClick.bind(this);
  }

  componentDidMount() {}

  onSelectItemChange(event) {
    const parsedValue = parseInt(event.target.value);
    this.setState({
      valveSessionLengthSec: parsedValue
    });
  }

  onOpenValveButtonClick() {
    axios.get(`./open_valve/?length_sec=${this.state.valveSessionLengthSec}`)
        .then((response) => {
          console.log(response);
        })
        .catch((error) => {
          console.error(error);
          alert(`${error}`);
        });
    this.setState({
      secondsTotal: this.state.valveSessionLengthSec,
      seconds: this.state.valveSessionLengthSec
    }, ()=>{
      this.startTimer();
    });
  }

  startTimer() {
    if (this.timer === 0 && this.state.seconds > 0) {
      this.timer = setInterval(this.countDown, 1000);
    }
  }

  countDown() {
    // Remove one second, set state so a re-render happens.
    const seconds = this.state.seconds - 1;
    this.setState({
      seconds: seconds
    });
    // Check if we're at zero.
    if (seconds === 0) {
      clearInterval(this.timer);
      this.timer = 0;
    }
  }

  render() {
    return (
      <>
        <Card sx={{maxWidth: 1280}}>
          <CardMedia
            component="img"
            image={this.state.imageEndpoint}
            sx={{objectFit: 'contain'}}
            onLoad={()=>{
              (new Promise((resolve) => setTimeout(resolve, 600))).then(() => {
                this.setState({imageEndpoint: './get_live_image_jpg/?' + Math.random()});
              });
            }}
          />
          <CardContent>
            <Box sx={{minWidth: 120}}>
              <Stack spacing={2} direction="row">
                <FormControl fullWidth>
                  <InputLabel id="select-duration-label">时长</InputLabel>
                  <Select
                    labelId="select-duration-label" id="simple-duration" value={this.state.valveSessionLengthSec}
                    label="时长" onChange={this.onSelectItemChange}
                  >
                    <MenuItem value={10}>10秒</MenuItem>
                    <MenuItem value={60}>60秒</MenuItem>
                    <MenuItem value={300}>5分钟</MenuItem>
                  </Select>
                </FormControl>
                <Button size="small" color="primary" onClick={this.onOpenValveButtonClick}>
                  开阀
                </Button>
              </Stack>
            </Box>
          </CardContent>
          <CardActions>
            <Box sx={{width: '100%'}}>
              <LinearProgressWithLabel
                value={(this.state.secondsTotal - this.state.seconds) / this.state.secondsTotal * 100}
                seconds={this.state.seconds} secondstotal={this.state.secondsTotal}
              />
            </Box>
          </CardActions>
        </Card>
      </>
    );
  }
}

class NavBar extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      user: null
    };
  }

  componentDidMount() {
    axios.get('../get_logged_in_user_json/')
        .then((response) => {
          this.setState({
            user: response.data.data
          });
        })
        .catch((error) => {
          console.error(error);
          alert(`${error}`);
        });
  }

  render() {
    return (
      <Box sx={{flexGrow: 1, mb: '1rem'}}>
        <AppBar position="static">
          <Toolbar>
            <GasMeterIcon sx={{display: {md: 'flex'}, mr: 1}} />
            <Typography
              variant="h6"
              noWrap
              component="a"
              href="/"
              sx={{
                mr: 2,
                display: {md: 'flex'},
                fontFamily: 'monospace',
                fontWeight: 700,
                letterSpacing: '.1rem',
                color: 'inherit',
                textDecoration: 'none'
              }}
            >
              水阀控制
            </Typography>
            <Typography variant="h6" component="div" sx={{flexGrow: 1}}>
            </Typography>
            <Button color="inherit">{this.state.user}</Button>
          </Toolbar>
        </AppBar>
      </Box>
    );
  }
}

class Index extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      currDate: new Date()
    };
  }

  render() {
    return (
      <>
        <NavBar />
        <div style={{maxWidth: '1440px', display: 'block', marginLeft: 'auto', marginRight: 'auto'}}>
          <Grid container>
            <Grid xs={12} md={8}>
              <Box display="flex" justifyContent="center" alignItems="center" mb='2rem'>
                <LiveImages />
              </Box>
            </Grid>
            <Grid xs={12} md={4} >
              <Box mb='2rem'>
              </Box>
            </Grid>
          </Grid>
        </div>
      </>
    );
  }
}

const container = document.getElementById('root');
ReactDOM.render(<Index />, container);
